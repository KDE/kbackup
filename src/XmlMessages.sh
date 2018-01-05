function get_files
{
    echo kbackup.xml
}

function po_for_file
{
    case "$1" in
       kbackup.xml)
           echo kbackup_xml_mimetypes.po
       ;;
    esac
}

function tags_for_file
{
    case "$1" in
       kbackup.xml)
           echo comment
       ;;
    esac
}
