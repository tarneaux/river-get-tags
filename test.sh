{
    
    cmd_list=(
        "./river-get-tags"
        "./river-get-tags -h"
        "./river-get-tags -f"
        "./river-get-tags -fo"
        "./river-get-tags -fon"
        "./river-get-tags -fom"
        "./river-get-tags -fonF9b"
        "./river-get-tags -fonF%9b"
        "./river-get-tags -fonF%90b"
        "./river-get-tags -fonF%d"
        "./river-get-tags -fonF%i"
        "./river-get-tags -fonF%u"
        "./river-get-tags -fonF%o"
        "./river-get-tags -fonF%x"
        "./river-get-tags -fonF%X"
        "./river-get-tags -fonF%#08x"
        "./river-get-tags -fonF%s"
        )


    for ((i = 0; i < ${#cmd_list[@]}; i++))
        do
            CMD=${cmd_list[$i]}
            echo "\$" $CMD "; echo \$?"
            $CMD ; echo $?
            echo ""
        done

} > test.log 2>&1