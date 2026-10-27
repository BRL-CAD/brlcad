/**
 *           N A M E _ S E L E C T I O N . H
 * BRL-CAD
 */

#ifndef CREO_SHARED_NAME_SELECTION_H
#define CREO_SHARED_NAME_SELECTION_H

#include <regex>
#include <string>
#include <vector>


class creo_brl_name_selector {
public:
    explicit creo_brl_name_selector(const std::string& text);

    const std::string& exact_name() const;
    int matches_exact(const std::string& parameter_name) const;
    int matches_pattern(const std::string& parameter_name) const;
    int has_pattern() const;

private:
    std::string exact_name_;
    std::regex pattern_;
    int has_pattern_;
};


std::string creo_brl_name_text_lower(const std::string& text);
std::string creo_brl_name_text_trim(const std::string& text);
std::vector<creo_brl_name_selector> creo_brl_parse_name_selectors(
    const std::string& text);


#endif /* CREO_SHARED_NAME_SELECTION_H */
