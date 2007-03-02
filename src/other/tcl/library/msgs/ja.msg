# created by tools/loadICU.tcl -- do not edit
namespace eval ::tcl::clock {
    ::msgcat::mcset ja DAYS_OF_WEEK_ABBREV [list \
        "\u65e5"\
        "\u6708"\
        "\u706b"\
        "\u6c34"\
        "\u6728"\
        "\u91d1"\
        "\u571f"]
    ::msgcat::mcset ja DAYS_OF_WEEK_FULL [list \
        "\u65e5\u66dc\u65e5"\
        "\u6708\u66dc\u65e5"\
        "\u706b\u66dc\u65e5"\
        "\u6c34\u66dc\u65e5"\
        "\u6728\u66dc\u65e5"\
        "\u91d1\u66dc\u65e5"\
        "\u571f\u66dc\u65e5"]
    ::msgcat::mcset ja MONTHS_ABBREV [list \
        "1"\
        "2"\
        "3"\
        "4"\
        "5"\
        "6"\
        "7"\
        "8"\
        "9"\
        "10"\
        "11"\
        "12"\
        ""]
    ::msgcat::mcset ja MONTHS_FULL [list \
        "1\u6708"\
        "2\u6708"\
        "3\u6708"\
        "4\u6708"\
        "5\u6708"\
        "6\u6708"\
        "7\u6708"\
        "8\u6708"\
        "9\u6708"\
        "10\u6708"\
        "11\u6708"\
        "12\u6708"\
        ""]
    ::msgcat::mcset ja BCE "\u7d00\u5143\u524d"
    ::msgcat::mcset ja CE "\u897f\u66a6"
    ::msgcat::mcset ja AM "\u5348\u524d"
    ::msgcat::mcset ja PM "\u5348\u5f8c"
    ::msgcat::mcset ja DATE_FORMAT "%Y/%m/%d"
    ::msgcat::mcset ja TIME_FORMAT "%k:%M:%S"
    ::msgcat::mcset ja DATE_TIME_FORMAT "%Y/%m/%d %k:%M:%S %z"
    ::msgcat::mcset ja LOCALE_NUMERALS "\u3007 \u4e00 \u4e8c \u4e09 \u56db \u4e94 \u516d \u4e03 \u516b \u4e5d \u5341 \u5341\u4e00 \u5341\u4e8c \u5341\u4e09 \u5341\u56db \u5341\u4e94 \u5341\u516d \u5341\u4e03 \u5341\u516b \u5341\u4e5d \u4e8c\u5341 \u5eff\u4e00 \u5eff\u4e8c \u5eff\u4e09 \u5eff\u56db \u5eff\u4e94 \u5eff\u516d \u5eff\u4e03 \u5eff\u516b \u5eff\u4e5d \u4e09\u5341 \u5345\u4e00 \u5345\u4e8c \u5345\u4e09 \u5345\u56db \u5345\u4e94 \u5345\u516d \u5345\u4e03 \u5345\u516b \u5345\u4e5d \u56db\u5341 \u56db\u5341\u4e00 \u56db\u5341\u4e8c \u56db\u5341\u4e09 \u56db\u5341\u56db \u56db\u5341\u4e94 \u56db\u5341\u516d \u56db\u5341\u4e03 \u56db\u5341\u516b \u56db\u5341\u4e5d \u4e94\u5341 \u4e94\u5341\u4e00 \u4e94\u5341\u4e8c \u4e94\u5341\u4e09 \u4e94\u5341\u56db \u4e94\u5341\u4e94 \u4e94\u5341\u516d \u4e94\u5341\u4e03 \u4e94\u5341\u516b \u4e94\u5341\u4e5d \u516d\u5341 \u516d\u5341\u4e00 \u516d\u5341\u4e8c \u516d\u5341\u4e09 \u516d\u5341\u56db \u516d\u5341\u4e94 \u516d\u5341\u516d \u516d\u5341\u4e03 \u516d\u5341\u516b \u516d\u5341\u4e5d \u4e03\u5341 \u4e03\u5341\u4e00 \u4e03\u5341\u4e8c \u4e03\u5341\u4e09 \u4e03\u5341\u56db \u4e03\u5341\u4e94 \u4e03\u5341\u516d \u4e03\u5341\u4e03 \u4e03\u5341\u516b \u4e03\u5341\u4e5d \u516b\u5341 \u516b\u5341\u4e00 \u516b\u5341\u4e8c \u516b\u5341\u4e09 \u516b\u5341\u56db \u516b\u5341\u4e94 \u516b\u5341\u516d \u516b\u5341\u4e03 \u516b\u5341\u516b \u516b\u5341\u4e5d \u4e5d\u5341 \u4e5d\u5341\u4e00 \u4e5d\u5341\u4e8c \u4e5d\u5341\u4e09 \u4e5d\u5341\u56db \u4e5d\u5341\u4e94 \u4e5d\u5341\u516d \u4e5d\u5341\u4e03 \u4e5d\u5341\u516b \u4e5d\u5341\u4e5d"
    ::msgcat::mcset ja LOCALE_DATE_FORMAT "%EY\u5e74%B%Od\u65e5"
    ::msgcat::mcset ja LOCALE_TIME_FORMAT "%OH\u6642%OM\u5206%OS\u79d2"
    ::msgcat::mcset ja LOCALE_DATE_TIME_FORMAT "%A %EY\u5e74%B%Od\u65e5%OH\u6642%OM\u5206%OS\u79d2 %z"
    ::msgcat::mcset ja LOCALE_ERAS "\u007b-9223372036854775808 \u897f\u66a6 0\u007d \u007b-3060979200 \u660e\u6cbb 1867\u007d \u007b-1812153600 \u5927\u6b63 1911\u007d \u007b-1357603200 \u662d\u548c 1925\u007d \u007b568512000 \u5e73\u6210 1987\u007d"
}
