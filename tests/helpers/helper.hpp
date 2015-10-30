#pragma once

#include <sstream>

#include <xlnt/s11n/xml_document.hpp>
#include <xlnt/s11n/xml_node.hpp>
#include <xlnt/s11n/xml_serializer.hpp>

#include "path_helper.hpp"

class Helper
{
public:
    enum class difference_type
    {
        names_differ,
        missing_attribute,
        attribute_values_differ,
        missing_text,
        text_values_differ,
        missing_child,
        child_order_differs,
        equivalent,
    };
    
    struct comparison_result
    {
        difference_type difference;
        std::string value_left;
        std::string value_right;
        operator bool() const { return difference == difference_type::equivalent; }
    };
    
    static comparison_result compare_xml(const std::string &expected, const xlnt::xml_document &observed)
    {
        std::ifstream f(expected);
        std::ostringstream s;
        f >> s.rdbuf();
        
        auto expected_xml = xlnt::xml_serializer::deserialize(s.str());
        
        return compare_xml(expected_xml.root(), observed.root());
    }
    
    static comparison_result compare_xml(const std::string &left_contents, const std::string &right_contents)
    {
        auto left_doc = xlnt::xml_serializer::deserialize(left_contents);
        auto right_doc = xlnt::xml_serializer::deserialize(right_contents);
        
        return compare_xml(left_doc.root(), right_doc.root());
    }
    
    static comparison_result compare_xml(const xlnt::xml_node &left, const xlnt::xml_node &right)
    {
        std::string left_temp = left.get_name();
        std::string right_temp = right.get_name();
        
        if(left_temp != right_temp)
        {
            return {difference_type::names_differ, left_temp, right_temp};
        }
        
        for(auto &left_attribute : left.get_attributes())
        {
            left_temp = left_attribute.second;
            
            if(!right.has_attribute(left_attribute.first))
            {
                return {difference_type::missing_attribute, left_temp, "((empty))"};
            }
            
            left_temp = left_attribute.second;
            right_temp = right.get_attribute(left_attribute.first);
            
            if(left_temp != right_temp)
            {
                return {difference_type::attribute_values_differ, left_temp, right_temp};
            }
        }
        
        if(left.has_text())
        {
            left_temp = left.get_text();
            
            if(!right.has_text())
            {
                return {difference_type::missing_text, left_temp, "((empty))"};
            }
            
            right_temp = right.get_text();
            
            if(left_temp != right_temp)
            {
                return {difference_type::text_values_differ, left_temp, right_temp};
            }
        }
        else if(right.has_text())
        {
            right_temp = right.get_text();
            return {difference_type::text_values_differ, "((empty))", right_temp};
        }
        
        auto right_child_iter = right.get_children().begin();
        
        for(auto left_child : left.get_children())
        {
            left_temp = left_child.get_name();
            
            if(right_child_iter == right.get_children().end())
            {
                return {difference_type::child_order_differs, left_temp, "((end))"};
            }
            
            auto right_child = *right_child_iter;
            right_child_iter++;
            
            auto child_comparison_result = compare_xml(left_child, right_child);
            
            if(!child_comparison_result)
            {
                return child_comparison_result;
            }
        }
        
        if(right_child_iter != right.get_children().end())
        {
            right_temp = right_child_iter->get_name();
            return {difference_type::child_order_differs, "((end))", right_temp};
        }
        
        return {difference_type::equivalent, "", ""};
    }
};
