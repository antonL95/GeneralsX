#pragma once

#include <algorithm>
#include <vector>

#include "Lib/Coord3D.h"
#include "WWMath/wwmath.h"

template <typename Member>
struct RetailProximityCluster
{
	Coord3D center{};
	Real radius = 0.0f;
	std::vector<Member> members;
};

template <typename Member, typename Position>
std::vector<RetailProximityCluster<Member>> buildRetailProximityClusters(
	const std::vector<Member>& members, Real nodeRadius, Position position)
{
	using Cluster = RetailProximityCluster<Member>;
	const auto add = [&](Cluster& cluster, const Member& member)
	{
		const Coord3D& point = position(member);
		if (cluster.members.empty())
		{
			cluster.center = point;
			cluster.radius = nodeRadius;
		}
		else
		{
			const Real distance = WWMath::SqrtfOrigin((point - cluster.center).lengthSqr());
			if (distance > cluster.radius - nodeRadius)
			{
				const Real nextRadius = (cluster.radius + distance + nodeRadius) * 0.5f;
				const Real fraction = (nextRadius - cluster.radius) / distance;
				cluster.center.x += (point.x - cluster.center.x) * fraction;
				cluster.center.y += (point.y - cluster.center.y) * fraction;
				cluster.center.z += (point.z - cluster.center.z) * fraction;
				cluster.radius = nextRadius;
			}
		}
		cluster.members.push_back(member);
	};

	std::vector<Cluster> clusters;
	const Real joinDistanceSquared = 2.0f * (nodeRadius * nodeRadius);
	for (const Member& member : members)
	{
		const Coord3D& point = position(member);
		const auto connected = [&](const Cluster& cluster)
		{
			return std::any_of(cluster.members.begin(), cluster.members.end(), [&](const Member& neighbor)
			{
				return (point - position(neighbor)).lengthSqr() < joinDistanceSquared;
			});
		};
		const auto first = std::find_if(clusters.begin(), clusters.end(), connected);
		if (first == clusters.end())
		{
			add(clusters.emplace_back(), member);
			continue;
		}
		add(*first, member);
		for (auto next = first + 1; next != clusters.end();)
		{
			if (!connected(*next))
			{
				++next;
				continue;
			}
			for (const Member& neighbor : next->members)
				add(*first, neighbor);
			next = clusters.erase(next);
		}
	}
	return clusters;
}
