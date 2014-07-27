#!/bin/bash

function help()
{
    cat <<-EOF
Code stats tools.
Usage code_stats <--options...>
options:
    --build     BUILD file
    --c++       c/c++ code
    --html      html
    --js        js code
    --llyy      lex and yacc
    --php       php code
    --proto     proto file
    --python    python code
    --swig      swig code
    --verbose   show verbose detail
    --help      show this information
EOF
}

function make_find_option()
{
    for type in $@; do
        echo -n " -or -name '*.$type'"
    done
}

function code_stats()
{
    local optargs
    if ! optargs=`getopt -n code_stats --long help,c++,proto,python,php,java,js,llyy,swig,js,html,build,verbose -- -- "$@"`; then
        return 1;
    fi
    verbose=""
    find_options=""
    #eval set "$optargs"
    while [ -n "$1" ]; do
        case $1 in
            --build)
                find_options="$find_options -or -name BUILD"
                shift;;
            --c++)
                find_options="$find_options `make_find_option h c cc cpp hh hpp`"
                shift;;
            --proto)
                find_options="$find_options `make_find_option proto`"
                shift;;
            --python)
                find_options="$find_options `make_find_option py`"
                shift;;
            --java)
                find_options="$find_options `make_find_option java`"
                shift;;
            --php)
                find_options="$find_options `make_find_option php`"
                shift;;
            --swig)
                find_options="$find_options `make_find_option i`"
                shift;;
            --llyy)
                find_options="$find_options `make_find_option l y ll yy`"
                shift;;
            --js)
                find_options="$find_options `make_find_option js`"
                shift;;
            --html)
                find_options="$find_options `make_find_option htm html`"
                shift;;
            --help)
                help
                return;;
            --verbose)
                verbose="yes"
                shift;;
            --*)
               echo "unknown option $1" >&2
               shift;
               exit;;
            *)
               break;;
        esac
    done

    if [ -z "$find_options" ]; then
        help
        exit 1
    fi
    find_options="-false "$find_options
    if [ -n "$verbose" ]; then
        eval find $@ $find_options | xargs wc -l
    else
        eval find $@ $find_options | xargs wc -l | tail -n1
    fi
}

code_stats "$@"
