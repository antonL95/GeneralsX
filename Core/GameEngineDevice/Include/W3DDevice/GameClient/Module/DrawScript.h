#pragma once

#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class DrawScript
{
public:
	struct Value
	{
		enum class Type { Nil, Boolean, Number, String };

		Type type = Type::Nil;
		bool boolean = false;
		double number = 0.0;
		std::string text;

		static Value of(bool value)
		{
			Value result;
			result.type = Type::Boolean;
			result.boolean = value;
			return result;
		}
		static Value of(double value)
		{
			Value result;
			result.type = Type::Number;
			result.number = value;
			return result;
		}
		static Value of(std::string value)
		{
			Value result;
			result.type = Type::String;
			result.text = std::move(value);
			return result;
		}
		bool truthy() const { return type != Type::Nil && (type != Type::Boolean || boolean); }
		bool operator==(const Value& other) const
		{
			if (type != other.type)
				return false;
			switch (type)
			{
				case Type::Boolean: return boolean == other.boolean;
				case Type::Number: return number == other.number;
				case Type::String: return text == other.text;
				default: return true;
			}
		}
	};

	class Host
	{
	public:
		virtual Value call(const std::string& function, const std::vector<Value>& arguments) = 0;

	protected:
		~Host() = default;
	};

	using Environment = std::map<std::string, Value>;

	static std::shared_ptr<const DrawScript> compile(std::string_view source)
	{
		Parser parser(source);
		std::shared_ptr<DrawScript> script(new DrawScript);
		if (!parser.tokenize() || !parser.block(script->m_body, 0) || !parser.atEnd())
			return nullptr;
		return script;
	}

	Value run(Host& host, Environment& variables) const
	{
		Machine machine{host, variables, {}, false};
		machine.execute(m_body);
		return machine.result;
	}

	Value run(Host& host) const
	{
		Environment variables;
		return run(host, variables);
	}

private:
	static constexpr int MAX_DEPTH = 64;

	struct Expression
	{
		enum class Kind { Constant, Variable, Call, Not, Negate, Binary };

		Kind kind = Kind::Constant;
		Value constant;
		std::string name;
		std::vector<Expression> operands;
	};

	struct Statement
	{
		enum class Kind { Call, Assign, If, Return };

		Kind kind = Kind::Call;
		std::string name;
		std::vector<Expression> expressions;
		std::vector<std::vector<Statement>> blocks;
	};

	struct Token
	{
		enum class Kind { End, Name, Number, String, Symbol };

		Kind kind = Kind::End;
		std::string text;
	};

	class Parser
	{
	public:
		explicit Parser(std::string_view source) : m_source(source) {}

		bool tokenize()
		{
			const std::size_t length = m_source.size();
			std::size_t i = 0;
			while (i < length)
			{
				const char c = m_source[i];
				if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
				{
					++i;
				}
				else if (m_source.substr(i, 2) == "--")
				{
					while (i < length && m_source[i] != '\n')
						++i;
				}
				else if (isNameStart(c))
				{
					const std::size_t start = i;
					while (i < length && (isNameStart(m_source[i]) || isDigit(m_source[i])))
						++i;
					push(Token::Kind::Name, m_source.substr(start, i - start));
				}
				else if (isDigit(c) || (c == '.' && i + 1 < length && isDigit(m_source[i + 1])))
				{
					const std::size_t start = i;
					while (i < length && (isDigit(m_source[i]) || m_source[i] == '.'))
						++i;
					push(Token::Kind::Number, m_source.substr(start, i - start));
				}
				else if (c == '"' || c == '\'')
				{
					const std::size_t start = ++i;
					while (i < length && m_source[i] != c && m_source[i] != '\n')
						++i;
					if (i >= length || m_source[i] != c)
						return false;
					push(Token::Kind::String, m_source.substr(start, i - start));
					++i;
				}
				else
				{
					const std::string_view pair = m_source.substr(i, 2);
					const bool two = pair == "==" || pair == "~=" || pair == "<=" || pair == ">=" || pair == "..";
					if (!two && std::string_view("=<>+-*/(),;").find(c) == std::string_view::npos)
						return false;
					push(Token::Kind::Symbol, m_source.substr(i, two ? 2 : 1));
					i += two ? 2 : 1;
				}
			}
			m_tokens.emplace_back();
			return true;
		}

		bool atEnd() const { return peek().kind == Token::Kind::End; }

		bool block(std::vector<Statement>& statements, int depth)
		{
			if (depth > MAX_DEPTH)
				return false;
			while (!atEnd() && !isWord("end") && !isWord("else") && !isWord("elseif"))
			{
				Statement statement;
				if (accept("if"))
				{
					statement.kind = Statement::Kind::If;
					do
					{
						statement.expressions.emplace_back();
						statement.blocks.emplace_back();
						if (!expression(statement.expressions.back(), 0, depth) || !accept("then") ||
							!block(statement.blocks.back(), depth + 1))
							return false;
					} while (accept("elseif"));
					if (accept("else"))
					{
						statement.blocks.emplace_back();
						if (!block(statement.blocks.back(), depth + 1))
							return false;
					}
					if (!accept("end"))
						return false;
				}
				else if (accept("return"))
				{
					statement.kind = Statement::Kind::Return;
					if (!atEnd() && !isWord("end") && !isWord("else") && !isWord("elseif") && !isWord(";"))
					{
						statement.expressions.emplace_back();
						if (!expression(statement.expressions.back(), 0, depth))
							return false;
					}
					accept(";");
					statements.push_back(std::move(statement));
					return atEnd() || isWord("end") || isWord("else") || isWord("elseif");
				}
				else
				{
					if (peek().kind != Token::Kind::Name || isReserved(peek().text))
						return false;
					statement.name = take().text;
					if (accept("="))
					{
						statement.kind = Statement::Kind::Assign;
						statement.expressions.emplace_back();
						if (!expression(statement.expressions.back(), 0, depth))
							return false;
					}
					else if (!arguments(statement.expressions, depth))
					{
						return false;
					}
				}
				accept(";");
				statements.push_back(std::move(statement));
			}
			return true;
		}

	private:
		static bool isNameStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
		static bool isDigit(char c) { return c >= '0' && c <= '9'; }
		static bool isReserved(const std::string& word)
		{
			for (const char* reserved : {"and", "do", "else", "elseif", "end", "for", "function", "if", "local",
				"nil", "not", "or", "repeat", "return", "then", "until", "while", "true", "false"})
			{
				if (word == reserved)
					return true;
			}
			return false;
		}
		static int precedence(const std::string& op)
		{
			if (op == "or") return 1;
			if (op == "and") return 2;
			if (op == "==" || op == "~=" || op == "<" || op == ">" || op == "<=" || op == ">=") return 3;
			if (op == "..") return 4;
			if (op == "+" || op == "-") return 5;
			if (op == "*" || op == "/") return 6;
			return 0;
		}

		void push(Token::Kind kind, std::string_view text) { m_tokens.push_back({kind, std::string(text)}); }
		const Token& peek() const { return m_tokens[m_next]; }
		Token take() { return atEnd() ? peek() : m_tokens[m_next++]; }
		bool isWord(const char* word) const
		{
			return (peek().kind == Token::Kind::Name || peek().kind == Token::Kind::Symbol) && peek().text == word;
		}
		bool accept(const char* word)
		{
			if (!isWord(word))
				return false;
			++m_next;
			return true;
		}

		bool arguments(std::vector<Expression>& list, int depth)
		{
			if (peek().kind == Token::Kind::String)
			{
				list.emplace_back();
				list.back().constant = Value::of(take().text);
				return true;
			}
			if (!accept("("))
				return false;
			if (accept(")"))
				return true;
			do
			{
				list.emplace_back();
				if (!expression(list.back(), 0, depth))
					return false;
			} while (accept(","));
			return accept(")");
		}

		bool primary(Expression& result, int depth)
		{
			if (depth > MAX_DEPTH)
				return false;
			if (accept("not") || accept("-"))
			{
				result.kind = m_tokens[m_next - 1].text == "not" ? Expression::Kind::Not : Expression::Kind::Negate;
				result.operands.emplace_back();
				return primary(result.operands.back(), depth + 1);
			}
			if (accept("("))
				return expression(result, 0, depth + 1) && accept(")");
			const Token token = take();
			if (token.kind == Token::Kind::String)
			{
				result.constant = Value::of(token.text);
				return true;
			}
			if (token.kind == Token::Kind::Number)
			{
				char* end = nullptr;
				result.constant = Value::of(std::strtod(token.text.c_str(), &end));
				return *end == '\0';
			}
			if (token.kind != Token::Kind::Name)
				return false;
			if (token.text == "nil")
				return true;
			if (token.text == "true" || token.text == "false")
			{
				result.constant = Value::of(token.text == "true");
				return true;
			}
			if (isReserved(token.text))
				return false;
			result.name = token.text;
			if (isWord("(") || peek().kind == Token::Kind::String)
			{
				result.kind = Expression::Kind::Call;
				return arguments(result.operands, depth + 1);
			}
			result.kind = Expression::Kind::Variable;
			return true;
		}

		bool expression(Expression& result, int minimum, int depth)
		{
			if (!primary(result, depth + 1))
				return false;
			for (int length = 0;; ++length)
			{
				const Token& next = peek();
				const int level = next.kind == Token::Kind::Name || next.kind == Token::Kind::Symbol ? precedence(next.text) : 0;
				if (level == 0 || level <= minimum)
					return true;
				if (length >= MAX_DEPTH)
					return false;
				Expression binary;
				binary.kind = Expression::Kind::Binary;
				binary.name = take().text;
				binary.operands.push_back(std::move(result));
				binary.operands.emplace_back();
				if (!expression(binary.operands.back(), level, depth + 1))
					return false;
				result = std::move(binary);
			}
		}

		std::string_view m_source;
		std::vector<Token> m_tokens;
		std::size_t m_next = 0;
	};

	struct Machine
	{
		enum class Flow { Next, Stop };

		Host& host;
		std::map<std::string, Value>& variables;

		Flow execute(const std::vector<Statement>& statements)
		{
			for (const Statement& statement : statements)
			{
				bool ok = true;
				switch (statement.kind)
				{
					case Statement::Kind::Call:
						call(statement.name, statement.expressions, ok);
						break;
					case Statement::Kind::Assign:
						variables[statement.name] = evaluate(statement.expressions.front(), ok);
						break;
					case Statement::Kind::Return:
					{
						if (!statement.expressions.empty())
						{
							const Value returned = evaluate(statement.expressions.front(), ok);
							if (ok)
								result = returned;
						}
						return Flow::Stop;
					}
					case Statement::Kind::If:
					{
						std::size_t branch = 0;
						while (branch < statement.expressions.size() && !evaluate(statement.expressions[branch], ok).truthy() && ok)
							++branch;
						if (ok && branch < statement.blocks.size() && execute(statement.blocks[branch]) == Flow::Stop)
							return Flow::Stop;
						break;
					}
				}
				if (!ok || failed)
				{
					failed = true;
					return Flow::Stop;
				}
			}
			return Flow::Next;
		}

		Value call(const std::string& name, const std::vector<Expression>& expressions, bool& ok)
		{
			std::vector<Value> arguments;
			for (const Expression& expression : expressions)
				arguments.push_back(evaluate(expression, ok));
			return ok ? host.call(name, arguments) : Value();
		}

		Value evaluate(const Expression& expression, bool& ok)
		{
			switch (expression.kind)
			{
				case Expression::Kind::Constant:
					return expression.constant;
				case Expression::Kind::Variable:
				{
					const auto found = variables.find(expression.name);
					return found == variables.end() ? Value() : found->second;
				}
				case Expression::Kind::Call:
					return call(expression.name, expression.operands, ok);
				case Expression::Kind::Not:
					return Value::of(!evaluate(expression.operands.front(), ok).truthy());
				case Expression::Kind::Negate:
				{
					const Value operand = evaluate(expression.operands.front(), ok);
					ok = ok && operand.type == Value::Type::Number;
					return Value::of(-operand.number);
				}
				case Expression::Kind::Binary:
					break;
			}
			const std::string& op = expression.name;
			const Value left = evaluate(expression.operands.front(), ok);
			if (op == "and")
				return left.truthy() ? evaluate(expression.operands.back(), ok) : left;
			if (op == "or")
				return left.truthy() ? left : evaluate(expression.operands.back(), ok);
			const Value right = evaluate(expression.operands.back(), ok);
			if (op == "==")
				return Value::of(left == right);
			if (op == "~=")
				return Value::of(!(left == right));
			if (op == "..")
			{
				ok = ok && left.type == Value::Type::String && right.type == Value::Type::String;
				return Value::of(left.text + right.text);
			}
			ok = ok && left.type == Value::Type::Number && right.type == Value::Type::Number;
			if (!ok)
				return Value();
			if (op == "<") return Value::of(left.number < right.number);
			if (op == ">") return Value::of(left.number > right.number);
			if (op == "<=") return Value::of(left.number <= right.number);
			if (op == ">=") return Value::of(left.number >= right.number);
			if (op == "+") return Value::of(left.number + right.number);
			if (op == "-") return Value::of(left.number - right.number);
			if (op == "*") return Value::of(left.number * right.number);
			ok = right.number != 0.0;
			return ok ? Value::of(left.number / right.number) : Value();
		}

		Value result;
		bool failed = false;
	};

	DrawScript() = default;

	std::vector<Statement> m_body;
};
