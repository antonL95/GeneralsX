check_animation() {
    frame=$1
    name=$2
    expected=$3
    if covers "$frame"; then
        line=$(grep -E "^TEST ability $frame Player_1 $name .* cond=" "$log") || return 1
        flags=,${line##* cond=},
        ability=0
        upgrade=0
        case "$flags" in *,SPECIAL_WEAPON_TWO,*) ability=1 ;; esac
        case "$flags" in *,USER_3,*) upgrade=1 ;; esac
        case "$expected:$ability:$upgrade" in
            preparing:0:0)
                case "$flags" in *,PREPARING,*) ;; *) printf '%s\n' "$line"; return 1 ;; esac
                ;;
            windup:1:1)
                case "$flags" in *,UNPACKING,*) ;; *) printf '%s\n' "$line"; return 1 ;; esac
                ;;
            overlap:1:1|upgrade:0:1|expired:0:0|latched:1:0) ;;
            *) printf '%s\n' "$line"; return 1 ;;
        esac
        printf '%s\n' "$line"
    fi
}

check() {
    check_animation 242 AbilityCustomDelayed upgrade &&
    check_animation 242 AbilityCustomNoUnpack preparing &&
    check_animation 254 AbilityCustomDelayed upgrade &&
    check_animation 260 AbilityCustomDelayed upgrade &&
    check_animation 264 AbilityCustomDelayed windup &&
    check_animation 274 AbilityCustomDelayed overlap &&
    check_animation 285 AbilityCustomDelayed upgrade &&
    check_animation 300 AbilityCustomDelayed expired &&
    check_animation 309 AbilityCustomLatched latched &&
    check_animation 328 AbilityCustomLatched expired
}
