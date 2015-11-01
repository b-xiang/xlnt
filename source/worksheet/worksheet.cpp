#include <algorithm>

#include <xlnt/cell/cell.hpp>
#include <xlnt/common/datetime.hpp>
#include <xlnt/common/exceptions.hpp>
#include <xlnt/common/relationship.hpp>
#include <xlnt/workbook/named_range.hpp>
#include <xlnt/workbook/workbook.hpp>
#include <xlnt/worksheet/range.hpp>
#include <xlnt/worksheet/range_reference.hpp>
#include <xlnt/worksheet/worksheet.hpp>

#include "detail/worksheet_impl.hpp"

namespace xlnt {

worksheet::worksheet() : d_(nullptr)
{
}

worksheet::worksheet(detail::worksheet_impl *d) : d_(d)
{
}

worksheet::worksheet(const worksheet &rhs) : d_(rhs.d_)
{
}

worksheet::worksheet(workbook &parent, const std::string &title)
    : d_(title == "" ? parent.create_sheet().d_ : parent.create_sheet(title).d_)
{
}

bool worksheet::has_frozen_panes() const
{
    return get_frozen_panes() != cell_reference("A1");
}

std::string worksheet::unique_sheet_name(const std::string &value) const
{
    auto names = get_parent().get_sheet_names();
    auto match = std::find(names.begin(), names.end(), value);
    std::size_t append = 0;
    while (match != names.end())
    {
        append++;
        match = std::find(names.begin(), names.end(), value + std::to_string(append));
    }
    return append == 0 ? value : value + std::to_string(append);
}

void worksheet::create_named_range(const std::string &name, const range_reference &reference)
{
    std::vector<named_range::target> targets;
    targets.push_back({ *this, reference });
    d_->named_ranges_[name] = named_range(name, targets);
}

range worksheet::operator()(const xlnt::cell_reference &top_left, const xlnt::cell_reference &bottom_right)
{
    return get_range({ top_left, bottom_right });
}

cell worksheet::operator[](const cell_reference &ref)
{
    return get_cell(ref);
}

std::vector<range_reference> worksheet::get_merged_ranges() const
{
    return d_->merged_cells_;
}

margins &worksheet::get_page_margins()
{
    return d_->page_margins_;
}

const margins &worksheet::get_page_margins() const
{
    return d_->page_margins_;
}

void worksheet::auto_filter(const range_reference &reference)
{
    d_->auto_filter_ = reference;
}

void worksheet::auto_filter(const xlnt::range &range)
{
    auto_filter(range.get_reference());
}

range_reference worksheet::get_auto_filter() const
{
    return d_->auto_filter_;
}

bool worksheet::has_auto_filter() const
{
    return d_->auto_filter_.get_width() > 0;
}

void worksheet::unset_auto_filter()
{
    d_->auto_filter_ = range_reference(1, 1, 1, 1);
}

page_setup &worksheet::get_page_setup()
{
    return d_->page_setup_;
}

const page_setup &worksheet::get_page_setup() const
{
    return d_->page_setup_;
}

std::string worksheet::to_string() const
{
    return "<Worksheet \"" + d_->title_ + "\">";
}

workbook &worksheet::get_parent() const
{
    return *d_->parent_;
}

void worksheet::garbage_collect()
{
    auto cell_map_iter = d_->cell_map_.begin();

    while (cell_map_iter != d_->cell_map_.end())
    {
        auto cell_iter = cell_map_iter->second.begin();

        while (cell_iter != cell_map_iter->second.end())
        {
            cell current_cell(&cell_iter->second);

            if (current_cell.garbage_collectible())
            {
                cell_iter = cell_map_iter->second.erase(cell_iter);
                continue;
            }

            cell_iter++;
        }

        if (cell_map_iter->second.empty())
        {
            cell_map_iter = d_->cell_map_.erase(cell_map_iter);
            continue;
        }

        cell_map_iter++;
    }
}

std::list<cell> worksheet::get_cell_collection()
{
    std::list<cell> cells;
    for (auto &c : d_->cell_map_)
    {
        for (auto &d : c.second)
        {
            cells.push_back(cell(&d.second));
        }
    }
    return cells;
}

std::string worksheet::get_title() const
{
    if (d_ == nullptr)
    {
        throw std::runtime_error("null worksheet");
    }
    return d_->title_;
}

void worksheet::set_title(const std::string &title)
{
    d_->title_ = title;
}

cell_reference worksheet::get_frozen_panes() const
{
    return d_->freeze_panes_;
}

void worksheet::freeze_panes(xlnt::cell top_left_cell)
{
    d_->freeze_panes_ = top_left_cell.get_reference();
}

void worksheet::freeze_panes(const std::string &top_left_coordinate)
{
    d_->freeze_panes_ = cell_reference(top_left_coordinate);
}

void worksheet::unfreeze_panes()
{
    d_->freeze_panes_ = cell_reference("A1");
}

cell worksheet::get_cell(const cell_reference &reference)
{
    if (d_->cell_map_.find(reference.get_row()) == d_->cell_map_.end())
    {
        d_->cell_map_[reference.get_row()] = std::unordered_map<column_t, detail::cell_impl>();
    }

    auto &row = d_->cell_map_[reference.get_row()];

    if (row.find(reference.get_column_index()) == row.end())
    {
        row[reference.get_column_index()] = detail::cell_impl(d_, reference.get_column_index(), reference.get_row());
    }

    return cell(&row[reference.get_column_index()]);
}

const cell worksheet::get_cell(const cell_reference &reference) const
{
    return cell(&d_->cell_map_.at(reference.get_row()).at(reference.get_column_index()));
}

bool worksheet::has_row_properties(row_t row) const
{
    return d_->row_properties_.find(row) != d_->row_properties_.end();
}

range worksheet::get_named_range(const std::string &name)
{
    if (!has_named_range(name))
    {
        throw named_range_exception();
    }

    return get_range(d_->named_ranges_[name].get_targets()[0].second);
}

column_t worksheet::get_lowest_column() const
{
    if (d_->cell_map_.empty())
    {
        return 1;
    }

    column_t lowest = std::numeric_limits<column_t>::max();

    for (auto &row : d_->cell_map_)
    {
        for (auto &c : row.second)
        {
            lowest = std::min(lowest, (column_t)c.first);
        }
    }

    return lowest;
}

row_t worksheet::get_lowest_row() const
{
    if (d_->cell_map_.empty())
    {
        return 1;
    }

    row_t lowest = std::numeric_limits<row_t>::max();

    for (auto &row : d_->cell_map_)
    {
        lowest = std::min(lowest, (row_t)row.first);
    }

    return lowest;
}

row_t worksheet::get_highest_row() const
{
    row_t highest = 1;

    for (auto &row : d_->cell_map_)
    {
        highest = std::max(highest, (row_t)row.first);
    }

    return highest;
}

column_t worksheet::get_highest_column() const
{
    column_t highest = 1;

    for (auto &row : d_->cell_map_)
    {
        for (auto &c : row.second)
        {
            highest = std::max(highest, (column_t)c.first);
        }
    }

    return highest;
}

range_reference worksheet::calculate_dimension() const
{
    auto lowest_column = get_lowest_column();
    auto lowest_row = get_lowest_row();

    auto highest_column = get_highest_column();
    auto highest_row = get_highest_row();

    return range_reference(lowest_column, lowest_row, highest_column, highest_row);
}

range worksheet::get_range(const range_reference &reference)
{
    return range(*this, reference);
}

const range worksheet::get_range(const range_reference &reference) const
{
    return range(*this, reference);
}

range worksheet::get_squared_range(column_t min_col, row_t min_row, column_t max_col, row_t max_row)
{
    range_reference reference(min_col, min_row, max_col, max_row);
    return get_range(reference);
}

const range worksheet::get_squared_range(column_t min_col, row_t min_row, column_t max_col, row_t max_row) const
{
    range_reference reference(min_col, min_row, max_col, max_row);
    return get_range(reference);
}

const std::vector<relationship> &worksheet::get_relationships() const
{
    return d_->relationships_;
}

relationship worksheet::create_relationship(relationship::type type, const std::string &target_uri)
{
    std::string r_id = "rId" + std::to_string(d_->relationships_.size() + 1);
    d_->relationships_.push_back(relationship(type, r_id, target_uri));
    return d_->relationships_.back();
}

void worksheet::merge_cells(const range_reference &reference)
{
    d_->merged_cells_.push_back(reference);
    bool first = true;

    for (auto row : get_range(reference))
    {
        for (auto cell : row)
        {
            cell.set_merged(true);

            if (!first)
            {
                if (cell.get_data_type() == cell::type::string)
                {
                    cell.set_value("");
                }
                else
                {
                    cell.clear_value();
                }
            }

            first = false;
        }
    }
}

void worksheet::merge_cells(column_t start_column, row_t start_row, column_t end_column, row_t end_row)
{
    merge_cells(xlnt::range_reference(start_column, start_row, end_column, end_row));
}

void worksheet::unmerge_cells(const range_reference &reference)
{
    auto match = std::find(d_->merged_cells_.begin(), d_->merged_cells_.end(), reference);

    if (match == d_->merged_cells_.end())
    {
        throw std::runtime_error("cells not merged");
    }

    d_->merged_cells_.erase(match);

    for (auto row : get_range(reference))
    {
        for (auto cell : row)
        {
            cell.set_merged(false);
        }
    }
}

void worksheet::unmerge_cells(column_t start_column, row_t start_row, column_t end_column, row_t end_row)
{
    unmerge_cells(xlnt::range_reference(start_column, start_row, end_column, end_row));
}

void worksheet::append()
{
    get_cell(cell_reference(1, get_next_row()));
}

void worksheet::append(const std::vector<std::string> &cells)
{
    xlnt::cell_reference next(1, get_next_row());

    for (auto cell : cells)
    {
        get_cell(next).set_value(cell);
        next.set_column_index(next.get_column_index() + 1);
    }
}

row_t worksheet::get_next_row() const
{
    auto row = get_highest_row() + 1;

    if (row == 2 && d_->cell_map_.size() == 0)
    {
        row = 1;
    }

    return row;
}

void worksheet::append(const std::vector<int> &cells)
{
    xlnt::cell_reference next(1, get_next_row());

    for (auto cell : cells)
    {
        get_cell(next).set_value(cell);
        next.set_column_index(next.get_column_index() + 1);
    }
}

void worksheet::append(const std::vector<date> &cells)
{
    xlnt::cell_reference next(1, get_next_row());

    for (auto cell : cells)
    {
        get_cell(next).set_value(cell);
        next.set_column_index(next.get_column_index() + 1);
    }
}

void worksheet::append(const std::vector<cell> &cells)
{
    xlnt::cell_reference next(1, get_next_row());

    for (auto cell : cells)
    {
        get_cell(next).set_value(cell);
        next.set_column_index(next.get_column_index() + 1);
    }
}

void worksheet::append(const std::unordered_map<std::string, std::string> &cells)
{
    auto row = get_next_row();

    for (auto cell : cells)
    {
        get_cell(cell_reference(cell.first, row)).set_value(cell.second);
    }
}

void worksheet::append(const std::unordered_map<int, std::string> &cells)
{
    auto row = get_next_row();

    for (auto cell : cells)
    {
        get_cell(cell_reference(static_cast<column_t>(cell.first), row)).set_value(cell.second);
    }
}

void worksheet::append(const std::vector<int>::const_iterator begin, const std::vector<int>::const_iterator end)
{
    xlnt::cell_reference next(1, get_next_row());

    for (auto i = begin; i != end; i++)
    {
        get_cell(next).set_value(*i);
        next.set_column_index(next.get_column_index() + 1);
    }
}

xlnt::range worksheet::rows() const
{
    return get_range(calculate_dimension());
}

xlnt::range worksheet::rows(const std::string &range_string) const
{
    return get_range(range_reference(range_string));
}

xlnt::range worksheet::rows(const std::string &range_string, int row_offset, int column_offset) const
{
    range_reference reference(range_string);
    return get_range(reference.make_offset(column_offset, row_offset));
}

xlnt::range worksheet::columns() const
{
    return range(*this, calculate_dimension(), major_order::column);
}

bool worksheet::operator==(const worksheet &other) const
{
    return d_ == other.d_;
}

bool worksheet::operator!=(const worksheet &other) const
{
    return d_ != other.d_;
}

bool worksheet::operator==(std::nullptr_t) const
{
    return d_ == nullptr;
}

bool worksheet::operator!=(std::nullptr_t) const
{
    return d_ != nullptr;
}

void worksheet::operator=(const worksheet &other)
{
    d_ = other.d_;
}

const cell worksheet::operator[](const cell_reference &ref) const
{
    return get_cell(ref);
}

range worksheet::operator[](const range_reference &ref)
{
    return get_range(ref);
}

range worksheet::operator[](const std::string &name)
{
    if (has_named_range(name))
    {
        return get_named_range(name);
    }
    return get_range(range_reference(name));
}

bool worksheet::has_named_range(const std::string &name)
{
    return d_->named_ranges_.find(name) != d_->named_ranges_.end();
}

void worksheet::remove_named_range(const std::string &name)
{
    if (!has_named_range(name))
    {
        throw std::runtime_error("worksheet doesn't have named range");
    }

    d_->named_ranges_.erase(name);
}

void worksheet::reserve(std::size_t n)
{
    d_->cell_map_.reserve(n);
}

void worksheet::increment_comments()
{
    d_->comment_count_++;
}

void worksheet::decrement_comments()
{
    d_->comment_count_--;
}

std::size_t worksheet::get_comment_count() const
{
    return d_->comment_count_;
}

header_footer &worksheet::get_header_footer()
{
    return d_->header_footer_;
}

const header_footer &worksheet::get_header_footer() const
{
    return d_->header_footer_;
}

header_footer::header_footer()
{
}

header::header() : default_(true), font_size_(12)
{
}

footer::footer() : default_(true), font_size_(12)
{
}

void worksheet::set_parent(xlnt::workbook &wb)
{
    d_->parent_ = &wb;
}

std::vector<std::string> worksheet::get_formula_attributes() const
{
    return {};
}

cell_reference worksheet::get_point_pos(int left, int top) const
{
    static const double DefaultColumnWidth = 51.85;
    static const double DefaultRowHeight = 15.0;

    auto points_to_pixels = [](double value, double dpi) { return (int)std::ceil(value * dpi / 72); };

    auto default_height = points_to_pixels(DefaultRowHeight, 96.0);
    auto default_width = points_to_pixels(DefaultColumnWidth, 96.0);

    column_t current_column = 1;
    row_t current_row = 1;

    int left_pos = 0;
    int top_pos = 0;

    while (left_pos <= left)
    {
        current_column++;

        if (has_column_properties(current_column))
        {
            auto cdw = get_column_properties(current_column).width;

            if (cdw >= 0)
            {
                left_pos += points_to_pixels(cdw, 96.0);
                continue;
            }
        }

        left_pos += default_width;
    }

    while (top_pos <= top)
    {
        current_row++;

        if (has_row_properties(current_row))
        {
            auto cdh = get_row_properties(current_row).height;

            if (cdh >= 0)
            {
                top_pos += points_to_pixels(cdh, 96.0);
                continue;
            }
        }

        top_pos += default_height;
    }

    return { current_column - 1, current_row - 1 };
}

cell_reference worksheet::get_point_pos(const std::pair<int, int> &point) const
{
    return get_point_pos(point.first, point.second);
}

void worksheet::set_sheet_state(page_setup::sheet_state state)
{
    get_page_setup().set_sheet_state(state);
}

void worksheet::add_column_properties(column_t column, const xlnt::column_properties &props)
{
    d_->column_properties_[column] = props;
}

bool worksheet::has_column_properties(column_t column) const
{
    return d_->column_properties_.find(column) != d_->column_properties_.end();
}

column_properties &worksheet::get_column_properties(column_t column)
{
    return d_->column_properties_[column];
}

const column_properties &worksheet::get_column_properties(column_t column) const
{
    return d_->column_properties_.at(column);
}

row_properties &worksheet::get_row_properties(row_t row)
{
    return d_->row_properties_[row];
}

const row_properties &worksheet::get_row_properties(row_t row) const
{
    return d_->row_properties_.at(row);
}

} // namespace xlnt
