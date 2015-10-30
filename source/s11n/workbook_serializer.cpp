#include <xlnt/s11n/workbook_serializer.hpp>
#include <xlnt/common/datetime.hpp>
#include <xlnt/common/exceptions.hpp>
#include <xlnt/common/relationship.hpp>
#include <xlnt/s11n/xml_document.hpp>
#include <xlnt/s11n/xml_node.hpp>
#include <xlnt/workbook/document_properties.hpp>
#include <xlnt/workbook/manifest.hpp>
#include <xlnt/workbook/named_range.hpp>
#include <xlnt/workbook/workbook.hpp>
#include <xlnt/worksheet/range_reference.hpp>
#include <xlnt/worksheet/worksheet.hpp>

#include "detail/constants.hpp"

namespace {
    
xlnt::datetime w3cdtf_to_datetime(const std::string &string)
{
    xlnt::datetime result(1900, 1, 1);
    auto separator_index = string.find('-');
    result.year = std::stoi(string.substr(0, separator_index));
    result.month = std::stoi(string.substr(separator_index + 1, string.find('-', separator_index + 1)));
    separator_index = string.find('-', separator_index + 1);
    result.day = std::stoi(string.substr(separator_index + 1, string.find('T', separator_index + 1)));
    separator_index = string.find('T', separator_index + 1);
    result.hour = std::stoi(string.substr(separator_index + 1, string.find(':', separator_index + 1)));
    separator_index = string.find(':', separator_index + 1);
    result.minute = std::stoi(string.substr(separator_index + 1, string.find(':', separator_index + 1)));
    separator_index = string.find(':', separator_index + 1);
    result.second = std::stoi(string.substr(separator_index + 1, string.find('Z', separator_index + 1)));
    return result;
}
    
std::string fill(const std::string &string, std::size_t length = 2)
{
    if(string.size() >= length)
    {
        return string;
    }
    
    return std::string(length - string.size(), '0') + string;
}

std::string datetime_to_w3cdtf(const xlnt::datetime &dt)
{
    return std::to_string(dt.year) + "-" + fill(std::to_string(dt.month)) + "-" + fill(std::to_string(dt.day)) + "T" + fill(std::to_string(dt.hour)) + ":" + fill(std::to_string(dt.minute)) + ":" + fill(std::to_string(dt.second)) + "Z";
}

} // namespace

namespace xlnt {
    
    /*
std::vector<std::pair<std::string, std::string>> workbook_serializer::read_sheets(zip_file &archive)
{
    std::string ns;
    
    for(auto child : doc.children())
    {
        std::string name = child.name();
        
        if(name.find(':') != std::string::npos)
        {
            auto colon_index = name.find(':');
            ns = name.substr(0, colon_index);
            break;
        }
    }
    
    auto with_ns = [&](const std::string &base) { return ns.empty() ? base : ns + ":" + base; };
    
    auto root_node = doc.get_child(with_ns("workbook"));
    auto sheets_node = root_node.get_child(with_ns("sheets"));
    
    std::vector<std::pair<std::string, std::string>> sheets;
    
    // store temp because pugixml iteration uses the internal char array multiple times
    auto sheet_element_name = with_ns("sheet");
    
    for(auto sheet_node : sheets_node.children(sheet_element_name))
    {
        std::string id = sheet_node.attribute("r:id").as_string();
        std::string name = sheet_node.attribute("name").as_string();
        sheets.push_back(std::make_pair(id, name));
    }
    
    return sheets;
}
     */

void workbook_serializer::read_properties_core(const xml_document &xml)
{
    auto &props = wb_.get_properties();
    auto root_node = xml.root();
    
    props.excel_base_date = calendar::windows_1900;
    
    if(root_node.has_child("dc:creator"))
    {
        props.creator = root_node.get_child("dc:creator").get_text();
    }
    if(root_node.has_child("cp:lastModifiedBy"))
    {
        props.last_modified_by = root_node.get_child("cp:lastModifiedBy").get_text();
    }
    if(root_node.has_child("dcterms:created"))
    {
        std::string created_string = root_node.get_child("dcterms:created").get_text();
        props.created = w3cdtf_to_datetime(created_string);
    }
    if(root_node.has_child("dcterms:modified"))
    {
        std::string modified_string = root_node.get_child("dcterms:modified").get_text();
        props.modified = w3cdtf_to_datetime(modified_string);
    }
}

std::string workbook_serializer::determine_document_type(const manifest &manifest)
{
    if(!manifest.has_override_type(constants::ArcWorkbook))
    {
        return "unsupported";
    }
    
    std::string type = manifest.get_override_type(constants::ArcWorkbook);
    
    if(type == "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml")
    {
        return "excel";
    }
    else if(type == "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml")
    {
        return "powerpoint";
    }
    else if(type == "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml")
    {
        return "word";
    }
    
    return "unsupported";
}

/// <summary>
/// Return a list of worksheets.
/// content types has a list of paths but no titles
/// workbook has a list of titles and relIds but no paths
/// workbook_rels has a list of relIds and paths but no titles
/// </summary>
std::vector<string_pair> workbook_serializer::detect_worksheets()
{
    static const std::string ValidWorksheet = "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml";
    
    std::vector<std::string> valid_sheets;
    
    for(const auto &content_type : wb_.get_manifest().get_override_types())
    {
        if(content_type.get_content_type() == ValidWorksheet)
        {
            valid_sheets.push_back(content_type.get_part_name());
        }
    }
    
    auto &workbook_relationships = wb_.get_relationships();
    std::vector<std::pair<std::string, std::string>> result;
    
    for(const auto &ws : read_sheets())
    {
        auto rel = *std::find_if(workbook_relationships.begin(), workbook_relationships.end(), [&](const relationship &r) { return r.get_id() == ws.first; });
        auto target = rel.get_target_uri();
        
        if(std::find(valid_sheets.begin(), valid_sheets.end(), "/" + target) != valid_sheets.end())
        {
            result.push_back({target, ws.second});
        }
    }
    
    return result;
}

xml_document workbook_serializer::write_properties_core() const
{
    auto &props = wb_.get_properties();
    
    xml_document xml;
    
    xml.add_namespace("cp", "http://schemas.openxmlformats.org/package/2006/metadata/core-properties");
    xml.add_namespace("dc", "http://purl.org/dc/elements/1.1/");
    xml.add_namespace("dcmitype", "http://purl.org/dc/dcmitype/");
    xml.add_namespace("dcterms", "http://purl.org/dc/terms/");
    xml.add_namespace("xsi", "http://www.w3.org/2001/XMLSchema-instance");
    
    auto &root_node = xml.root();
    root_node.set_name("cp:coreProperties");
    
    root_node.add_child("dc:creator").set_text(props.creator);
    root_node.add_child("cp:lastModifiedBy").set_text(props.last_modified_by);
    root_node.add_child("dcterms:created").set_text(datetime_to_w3cdtf(props.created));
    root_node.get_child("dcterms:created").add_attribute("xsi:type", "dcterms:W3CDTF");
    root_node.add_child("dcterms:modified").set_text(datetime_to_w3cdtf(props.modified));
    root_node.get_child("dcterms:modified").add_attribute("xsi:type", "dcterms:W3CDTF");
    root_node.add_child("dc:title").set_text(props.title);
    root_node.add_child("dc:description");
    root_node.add_child("dc:subject");
    root_node.add_child("cp:keywords");
    root_node.add_child("cp:category");
    
    return xml;
}
    
xml_document workbook_serializer::write_properties_app() const
{
    xml_document xml;
    
    xml.add_namespace("xmlns", "http://schemas.openxmlformats.org/officeDocument/2006/extended-properties");
    xml.add_namespace("xmlns:vt", "http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes");
    
    auto &root_node = xml.root();
    root_node.set_name("Properties");
    
    root_node.add_child("Application").set_text("Microsoft Excel");
    root_node.add_child("DocSecurity").set_text("0");
    root_node.add_child("ScaleCrop").set_text("false");
    root_node.add_child("Company");
    root_node.add_child("LinksUpToDate").set_text("false");
    root_node.add_child("SharedDoc").set_text("false");
    root_node.add_child("HyperlinksChanged").set_text("false");
    root_node.add_child("AppVersion").set_text("12.0000");
    
    auto heading_pairs_node = root_node.add_child("HeadingPairs");
    auto heading_pairs_vector_node = heading_pairs_node.add_child("vt:vector");
    heading_pairs_vector_node.add_attribute("baseType", "variant");
    heading_pairs_vector_node.add_attribute("size", "2");
    heading_pairs_vector_node.add_child("vt:variant").add_child("vt:lpstr").set_text("Worksheets");
    heading_pairs_vector_node.add_child("vt:variant").add_child("vt:i4").set_text(std::to_string(wb_.get_sheet_names().size()));
    
    auto titles_of_parts_node = root_node.add_child("TitlesOfParts");
    auto titles_of_parts_vector_node = titles_of_parts_node.add_child("vt:vector");
    titles_of_parts_vector_node.add_attribute("baseType", "lpstr");
    titles_of_parts_vector_node.add_attribute("size", std::to_string(wb_.get_sheet_names().size()));
    
    for(auto ws : wb_)
    {
        titles_of_parts_vector_node.add_child("vt:lpstr").set_text(ws.get_title());
    }

    return xml;
}

xml_document workbook_serializer::write_workbook() const
{
    std::size_t num_visible = 0;
    
    for(auto ws : wb_)
    {
        if(ws.get_page_setup().get_sheet_state() == xlnt::page_setup::sheet_state::visible)
        {
            num_visible++;
        }
    }
    
    if(num_visible == 0)
    {
        throw xlnt::value_error();
    }
    
    xml_document xml;
    
    xml.add_namespace("xmlns", "http://schemas.openxmlformats.org/spreadsheetml/2006/main");
    xml.add_namespace("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
    
    auto &root_node = xml.root();
    root_node.set_name("workbook");
    
    auto &file_version_node = root_node.add_child("fileVersion");
    file_version_node.add_attribute("appName", "xl");
    file_version_node.add_attribute("lastEdited", "4");
    file_version_node.add_attribute("lowestEdited", "4");
    file_version_node.add_attribute("rupBuild", "4505");
    
    auto &workbook_pr_node = root_node.add_child("workbookPr");
    workbook_pr_node.add_attribute("codeName", "ThisWorkbook");
    workbook_pr_node.add_attribute("defaultThemeVersion", "124226");
    workbook_pr_node.add_attribute("date1904", wb_.get_properties().excel_base_date == calendar::mac_1904 ? "1" : "0");
    
    auto book_views_node = root_node.add_child("bookViews");
    auto workbook_view_node = book_views_node.add_child("workbookView");
    workbook_view_node.add_attribute("activeTab", "0");
    workbook_view_node.add_attribute("autoFilterDateGrouping", "1");
    workbook_view_node.add_attribute("firstSheet", "0");
    workbook_view_node.add_attribute("minimized", "0");
    workbook_view_node.add_attribute("showHorizontalScroll", "1");
    workbook_view_node.add_attribute("showSheetTabs", "1");
    workbook_view_node.add_attribute("showVerticalScroll", "1");
    workbook_view_node.add_attribute("tabRatio", "600");
    workbook_view_node.add_attribute("visibility", "visible");
    
    auto sheets_node = root_node.add_child("sheets");
    auto defined_names_node = root_node.add_child("definedNames");
    
    for(auto relationship : wb_.get_relationships())
    {
        if(relationship.get_type() == relationship::type::worksheet)
        {
            std::string sheet_index_string = relationship.get_target_uri();
            sheet_index_string = sheet_index_string.substr(0, sheet_index_string.find('.'));
            sheet_index_string = sheet_index_string.substr(sheet_index_string.find_last_of('/'));
            auto iter = sheet_index_string.end();
            iter--;
            while (isdigit(*iter)) iter--;
            auto first_digit = iter - sheet_index_string.begin();
            sheet_index_string = sheet_index_string.substr(static_cast<std::string::size_type>(first_digit + 1));
            std::size_t sheet_index = static_cast<std::size_t>(std::stoll(sheet_index_string) - 1);
            
            auto ws = wb_.get_sheet_by_index(sheet_index);
            
            auto sheet_node = sheets_node.add_child("sheet");
            sheet_node.add_attribute("name", ws.get_title());
            sheet_node.add_attribute("r:id", relationship.get_id());
            sheet_node.add_attribute("sheetId", std::to_string(sheet_index + 1));
            
            if(ws.has_auto_filter())
            {
                auto &defined_name_node = defined_names_node.add_child("definedName");
                defined_name_node.add_attribute("name", "_xlnm._FilterDatabase");
                defined_name_node.add_attribute("hidden", "1");
                defined_name_node.add_attribute("localSheetId", "0");
                std::string name = "'" + ws.get_title() + "'!" + range_reference::make_absolute(ws.get_auto_filter()).to_string();
                defined_name_node.set_text(name);
            }
        }
    }
    
    auto calc_pr_node = root_node.add_child("calcPr");
    calc_pr_node.add_attribute("calcId", "124519");
    calc_pr_node.add_attribute("calcMode", "auto");
    calc_pr_node.add_attribute("fullCalcOnLoad", "1");
    
    return xml;
}
    
bool workbook_serializer::write_named_ranges(xlnt::xml_node &named_ranges_node)
{
    for(auto &named_range : wb_.get_named_ranges())
    {
        named_ranges_node.add_child(named_range.get_name());
    }

    return true;
}

} // namespace xlnt
