#pragma once
#include "revenge/loader.hpp"
namespace revenge {
std::string compatibility_report(const LoadPlan&,const std::filesystem::path&);
std::string compatibility_file(const std::filesystem::path&);
std::string target_plan_report(const LoadPlan&,const std::filesystem::path&);
}
