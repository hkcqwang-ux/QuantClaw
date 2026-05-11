// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include "quantclaw/skill/skill_parse_file.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stack>

#include <spdlog/spdlog.h>

namespace quantclaw {

// Skill 文件解析器实现

std::string SkillParser::ReadFileContent(const std::string& skill_file) {
  std::ifstream file(skill_file);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open skill file: " + skill_file);
  }

  std::ostringstream content;
  content << file.rdbuf();
  file.close();
  return content.str();
}

bool SkillParser::ExtractFrontmatter(const std::string& content, std::string& frontmatter) {
  std::regex frontmatter_regex(R"(^---\s*\n([\s\S]*?)\n---\s*\n)");
  if (std::regex_search(content, frontmatter_matches_, frontmatter_regex)) {
    frontmatter = frontmatter_matches_[1].str();
    return true;
  }
  return false;
}

std::vector<std::string> SkillParser::ExtractStringArray(const nlohmann::json& arr) {
  std::vector<std::string> result;
  if (!arr.is_array()) return result;
  for (const auto& item : arr) {
    if (item.is_string()) {
      result.push_back(item.get<std::string>());
    } else if (item.is_object()) {
      if (item.size() == 1) {
        auto it = item.begin();
        std::string reconstructed = it.key() + ":";
        if (it.value().is_string()) {
          reconstructed += it.value().get<std::string>();
        }
        result.push_back(reconstructed);
      }
    }
  }
  return result;
}

void SkillParser::ExtractRequires(const nlohmann::json& reqs) {
  if (reqs.contains("bins")) {
    skill_.extra.required_bins = ExtractStringArray(reqs["bins"]);
  }
  if (reqs.contains("env")) {
    skill_.extra.required_envs = ExtractStringArray(reqs["env"]);
  }
  if (reqs.contains("envs")) {
    skill_.extra.required_envs = ExtractStringArray(reqs["envs"]);
  }
  if (reqs.contains("anyBins")) {
    skill_.extra.any_bins = ExtractStringArray(reqs["anyBins"]);
  }
  if (reqs.contains("config")) {
    skill_.extra.config_files = ExtractStringArray(reqs["config"]);
  }
}

void SkillParser::ParseMiniMetadata(const nlohmann::json& metadata) {
  if (metadata.contains("name")) {
    skill_.mini.name = metadata["name"].get<std::string>();
  }
  if (metadata.contains("description")) {
    skill_.mini.description = metadata["description"].get<std::string>();
  }
  if (metadata.contains("emoji")) {
    skill_.mini.emoji = metadata["emoji"].get<std::string>();
  }
  if (metadata.contains("always")) {
    if (metadata["always"].is_boolean()) {
      skill_.mini.always = metadata["always"].get<bool>();
    } else {
      skill_.mini.always = metadata["always"].get<std::string>() == "true";
    }
  }
  if (metadata.contains("os")) {
    if (metadata["os"].is_array()) {
      skill_.mini.os_restrict = ExtractStringArray(metadata["os"]);
    }
  }
}

void SkillParser::ParseRequires(const nlohmann::json& metadata) {
  if (metadata.contains("requires") && metadata["requires"].is_object()) {
    ExtractRequires(metadata["requires"]);
  }
  if (metadata.contains("metadata") &&
      metadata["metadata"].contains("openclaw") &&
      metadata["metadata"]["openclaw"].contains("requires")) {
    ExtractRequires(metadata["metadata"]["openclaw"]["requires"]);
  }

  if (skill_.extra.required_envs.empty() && metadata.contains("env") &&
      metadata["env"].is_array()) {
    skill_.extra.required_envs = ExtractStringArray(metadata["env"]);
  }
  if (skill_.extra.required_bins.empty() && metadata.contains("bins") &&
      metadata["bins"].is_array()) {
    skill_.extra.required_bins = ExtractStringArray(metadata["bins"]);
  }
  if (skill_.extra.any_bins.empty() && metadata.contains("anyBins") &&
      metadata["anyBins"].is_array()) {
    skill_.extra.any_bins = ExtractStringArray(metadata["anyBins"]);
  }
  if (skill_.extra.config_files.empty() && metadata.contains("config") &&
      metadata["config"].is_array()) {
    skill_.extra.config_files = ExtractStringArray(metadata["config"]);
  }
}

void SkillParser::ParseExtraMetadata(const nlohmann::json& metadata) {
  if (metadata.contains("homepage")) {
    skill_.extra.homepage = metadata["homepage"].get<std::string>();
  }
  if (metadata.contains("skillKey")) {
    skill_.extra.skill_key = metadata["skillKey"].get<std::string>();
  }
  if (metadata.contains("primaryEnv")) {
    skill_.extra.primary_env = metadata["primaryEnv"].get<std::string>();
  }
}

void SkillParser::ParseOpenclawFields(const nlohmann::json& metadata) {
  if (metadata.contains("metadata") &&
      metadata["metadata"].contains("openclaw")) {
    const auto& oc = metadata["metadata"]["openclaw"];
    if (oc.contains("emoji") && skill_.mini.emoji.empty()) {
      skill_.mini.emoji = oc["emoji"].get<std::string>();
    }
    if (oc.contains("homepage") && skill_.extra.homepage.empty()) {
      skill_.extra.homepage = oc["homepage"].get<std::string>();
    }
    if (oc.contains("skillKey") && skill_.extra.skill_key.empty()) {
      skill_.extra.skill_key = oc["skillKey"].get<std::string>();
    }
    if (oc.contains("primaryEnv") && skill_.extra.primary_env.empty()) {
      skill_.extra.primary_env = oc["primaryEnv"].get<std::string>();
    }
    if (oc.contains("always") && !skill_.mini.always) {
      if (oc["always"].is_boolean()) {
        skill_.mini.always = oc["always"].get<bool>();
      }
    }
    if (oc.contains("os") && skill_.mini.os_restrict.empty()) {
      if (oc["os"].is_array()) {
        skill_.mini.os_restrict = ExtractStringArray(oc["os"]);
      }
    }
  }
}

void SkillParser::ParseExecutionContext(const nlohmann::json& metadata) {
  // Support both camelCase (allowedTools) and snake_case (allowed_tools)
  if (metadata.contains("allowedTools") && metadata["allowedTools"].is_array()) {
    skill_.extra.allowed_tools = ExtractStringArray(metadata["allowedTools"]);
  } else if (metadata.contains("allowed_tools") && metadata["allowed_tools"].is_array()) {
    skill_.extra.allowed_tools = ExtractStringArray(metadata["allowed_tools"]);
  }
  
  // Support both "model" and "model_override" field names
  if (metadata.contains("model") && skill_.extra.model_override.empty()) {
    skill_.extra.model_override = metadata["model"].get<std::string>();
  } else if (metadata.contains("model_override") && skill_.extra.model_override.empty()) {
    skill_.extra.model_override = metadata["model_override"].get<std::string>();
  }
  
  if (metadata.contains("metadata") &&
      metadata["metadata"].contains("openclaw")) {
    const auto& oc = metadata["metadata"]["openclaw"];
    if (oc.contains("allowedTools") && skill_.extra.allowed_tools.empty() &&
        oc["allowedTools"].is_array()) {
      skill_.extra.allowed_tools = ExtractStringArray(oc["allowedTools"]);
    }
    if (oc.contains("model") && skill_.extra.model_override.empty()) {
      skill_.extra.model_override = oc["model"].get<std::string>();
    }
  }
}

void SkillParser::ParseInstallInfo(const nlohmann::json& metadata) {
  const nlohmann::json* install_section = nullptr;
  if (metadata.contains("metadata") &&
      metadata["metadata"].contains("openclaw") &&
      metadata["metadata"]["openclaw"].contains("install")) {
    install_section = &metadata["metadata"]["openclaw"]["install"];
  } else if (metadata.contains("install")) {
    install_section = &metadata["install"];
  }
  if (!install_section) return;

  if (install_section->is_array()) {
    for (const auto& entry : *install_section) {
      if (!entry.is_object()) continue;
      SkillInstallInfo info;
      info.kind = entry.value("kind", "");
      info.method = info.kind;
      info.id = entry.value("id", "");
      info.label = entry.value("label", "");
      info.formula = entry.value("formula", "");
      info.package = entry.value("package", "");
      info.module = entry.value("module", "");
      info.url = entry.value("url", "");
      info.archive = entry.value("archive", "");
      info.target_dir = entry.value("targetDir", "");
      info.extract = entry.value("extract", false);
      info.strip_components = entry.value("stripComponents", 0);
      if (entry.contains("bins") && entry["bins"].is_array()) {
        info.bins = entry["bins"].get<std::vector<std::string>>();
        if (info.binary.empty() && !info.bins.empty()) {
          info.binary = info.bins.front();
        }
      }
      if (entry.contains("os") && entry["os"].is_array()) {
        info.os = entry["os"].get<std::vector<std::string>>();
      }
      skill_.extra.installs.push_back(std::move(info));
    }
  } else if (install_section->is_object()) {
    for (auto it = install_section->begin(); it != install_section->end(); ++it) {
      SkillInstallInfo info;
      info.method = it.key();
      info.kind = it.key();
      if (it.value().is_string()) {
        info.formula = it.value().get<std::string>();
      } else if (it.value().is_object()) {
        info.formula = it.value().value("formula", "");
        info.binary = it.value().value("binary", "");
        info.package = it.value().value("package", "");
        info.url = it.value().value("url", "");
        if (it.value().contains("bins") && it.value()["bins"].is_array()) {
          info.bins = it.value()["bins"].get<std::vector<std::string>>();
        }
      }
      skill_.extra.installs.push_back(std::move(info));
    }
  }
}

void SkillParser::ParseCommands(const nlohmann::json& metadata) {
  const nlohmann::json* cmds_section = nullptr;
  if (metadata.contains("commands") && metadata["commands"].is_array()) {
    cmds_section = &metadata["commands"];
  } else if (metadata.contains("metadata") &&
             metadata["metadata"].contains("openclaw") &&
             metadata["metadata"]["openclaw"].contains("commands") &&
             metadata["metadata"]["openclaw"]["commands"].is_array()) {
    cmds_section = &metadata["metadata"]["openclaw"]["commands"];
  }
  if (!cmds_section) return;

  for (const auto& cmd : *cmds_section) {
    if (!cmd.is_object()) continue;
    SkillCommand sc;
    sc.name = cmd.value("name", "");
    sc.description = cmd.value("description", "");
    sc.tool_name = cmd.value("toolName", cmd.value("tool", ""));
    sc.arg_mode = cmd.value("argMode", cmd.value("arg_mode", "freeform"));
    if (!sc.name.empty()) {
      skill_.extra.commands.push_back(std::move(sc));
    }
  }
}

void SkillParser::SetDefaultName(const std::string& skill_file) {
  if (skill_.mini.name.empty()) {
    skill_.mini.name = std::filesystem::path(skill_file).parent_path().filename().string();
  }
}

std::string SkillParser::CheckResourceDir(const std::string& root_dir, const std::string& subdir) {
  auto p = std::filesystem::path(root_dir) / subdir;
  if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
    return p.string();
  }
  return "";
}

void SkillParser::SetResourceDirs(const std::string& skill_file) {
  skill_.mini.root_dir = std::filesystem::path(skill_file).parent_path().string();
  skill_.mini.scripts_dir = CheckResourceDir(skill_.mini.root_dir, "scripts");
  skill_.mini.references_dir = CheckResourceDir(skill_.mini.root_dir, "references");
  skill_.mini.assets_dir = CheckResourceDir(skill_.mini.root_dir, "assets");
}

void SkillParser::ExtractContent(const std::string& content, bool load_content) {
  if (load_content) {
    if (!frontmatter_matches_.empty()) {
      skill_.body.content = content.substr(static_cast<size_t>(frontmatter_matches_[0].length()));
    } else {
      skill_.body.content = content;
    }
  } else {
    skill_.body.content.clear();
  }
}

SkillFullData SkillParser::ParseSkillFile(const std::string& skill_file,
                                          bool load_content) {
  skill_ = SkillFullData();
  
  std::string content = ReadFileContent(skill_file);
  std::string frontmatter;
  
  if (ExtractFrontmatter(content, frontmatter)) {
    try {
      nlohmann::json metadata = ParseYamlFrontmatter(frontmatter);
      
      ParseMiniMetadata(metadata);
      ParseRequires(metadata);
      ParseExtraMetadata(metadata);
      ParseOpenclawFields(metadata);
      ParseExecutionContext(metadata);
      ParseInstallInfo(metadata);
      ParseCommands(metadata);
    } catch (const std::exception& e) {
      spdlog::error("[YAML Parse Error] Failed to parse frontmatter: {}", e.what());
      spdlog::error("[YAML Parse Error] Frontmatter content:\n{}", frontmatter);
    }
  }
  
  SetDefaultName(skill_file);
  SetResourceDirs(skill_file);
  ExtractContent(content, load_content);
  
  return skill_;
}

// YAML 解析方法实现

SkillParser::ParsedLine SkillParser::ParseYamlLine(const std::string& line) {
  ParsedLine result;
  result.is_empty = (line.find_first_not_of(" \t\r\n") == std::string::npos);
  if (result.is_empty) return result;
  
  result.indent = 0;
  for (char c : line) {
    if (c == ' ') ++result.indent;
    else if (c == '\t') result.indent += 2;
    else break;
  }
  
  result.trimmed = line.substr(line.find_first_not_of(" \t"));
  while (!result.trimmed.empty() &&
         (result.trimmed.back() == ' ' || result.trimmed.back() == '\t' ||
          result.trimmed.back() == '\r')) {
    result.trimmed.pop_back();
  }
  
  return result;
}

std::string SkillParser::CollapseYamlSpaces(const std::string& text) {
  std::string result;
  bool last_was_space = false;
  for (char c : text) {
    if (c == ' ') {
      if (!last_was_space) result += c;
      last_was_space = true;
    } else {
      result += c;
      last_was_space = false;
    }
  }
  return result;
}

void SkillParser::FinalizeYamlMultilineString() {
  if (yaml_ctx_.multiline_fold) {
    while (!yaml_ctx_.multiline_value.empty() && yaml_ctx_.multiline_value.back() == ' ') {
      yaml_ctx_.multiline_value.pop_back();
    }
    yaml_ctx_.multiline_value = CollapseYamlSpaces(yaml_ctx_.multiline_value);
  }
  
  if (yaml_ctx_.pending_parent && !yaml_ctx_.multiline_key.empty()) {
    (*yaml_ctx_.pending_parent)[yaml_ctx_.multiline_key] = yaml_ctx_.multiline_value;
  }
  
  yaml_ctx_.multiline_key.clear();
  yaml_ctx_.multiline_value.clear();
}

bool SkillParser::HandleYamlMultilineContinuation(const ParsedLine& parsed) {
  if (!yaml_ctx_.in_multiline) return false;
  
  if (parsed.indent > yaml_ctx_.multiline_indent) {
    if (!yaml_ctx_.multiline_fold) {
      yaml_ctx_.multiline_value += "\n" + parsed.trimmed;
    } else {
      if (!yaml_ctx_.multiline_value.empty() && yaml_ctx_.multiline_value.back() != ' ') {
        yaml_ctx_.multiline_value += " ";
      }
      yaml_ctx_.multiline_value += parsed.trimmed;
    }
    return true;
  }
  
  yaml_ctx_.in_multiline = false;
  FinalizeYamlMultilineString();
  return false;
}

void SkillParser::UpdateYamlContextStack(int indent) {
  while (yaml_ctx_.ctx_stack.size() > 1 && yaml_ctx_.ctx_stack.top().first > indent) {
    yaml_ctx_.ctx_stack.pop();
  }
}

void SkillParser::HandleYamlPendingKey(const ParsedLine& parsed) {
  if (yaml_ctx_.pending_key.empty()) return;
  
  if (parsed.indent > yaml_ctx_.pending_indent) {
    if (parsed.trimmed[0] == '-') {
      (*yaml_ctx_.pending_parent)[yaml_ctx_.pending_key] = nlohmann::json::array();
    } else {
      (*yaml_ctx_.pending_parent)[yaml_ctx_.pending_key] = nlohmann::json::object();
      yaml_ctx_.ctx_stack.push({parsed.indent, &(*yaml_ctx_.pending_parent)[yaml_ctx_.pending_key]});
    }
  } else {
    (*yaml_ctx_.pending_parent)[yaml_ctx_.pending_key] = "";
  }
  
  yaml_ctx_.pending_key.clear();
}

void SkillParser::FinalizeYamlPendingKey() {
  if (!yaml_ctx_.pending_key.empty() && yaml_ctx_.pending_parent) {
    (*yaml_ctx_.pending_parent)[yaml_ctx_.pending_key] = "";
  }
}

void SkillParser::ParseYamlArrayItem(const ParsedLine& parsed) {
  std::string val = parsed.trimmed.substr(1);
  val.erase(0, val.find_first_not_of(" \t"));
  if (val.empty()) return;
  
  if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
    val = val.substr(1, val.size() - 2);
  }
  
  nlohmann::json* current = yaml_ctx_.ctx_stack.top().second;
  nlohmann::json* arr = nullptr;
  for (auto it = current->begin(); it != current->end(); ++it) {
    if (it->is_array()) {
      arr = &(*it);
    }
  }
  
  if (!arr) return;
  
  if (IsYamlObjectArrayItem(val)) {
    size_t colon_pos = val.find(':');
    std::string key = val.substr(0, colon_pos);
    std::string value = val.substr(colon_pos + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
      value.pop_back();
    }
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
    }
    
    arr->push_back(nlohmann::json::object());
    if (!value.empty()) {
      arr->back()[key] = value;
    }
    yaml_ctx_.ctx_stack.push({parsed.indent + 1, &arr->back()});
  } else {
    arr->push_back(val);
  }
}

bool SkillParser::IsYamlObjectArrayItem(const std::string& val) {
  size_t colon_pos = val.find(':');
  if (colon_pos == std::string::npos || colon_pos == 0) return false;
  
  char after = (colon_pos + 1 < val.size()) ? val[colon_pos + 1] : ' ';
  if (after != ' ' && after != '\t') return false;
  
  for (size_t i = 0; i < colon_pos; ++i) {
    char c = val[i];
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
      return false;
    }
  }
  
  char first_char = val[0];
  return std::isalpha(static_cast<unsigned char>(first_char)) || first_char == '_';
}

void SkillParser::ParseYamlKeyValue(const ParsedLine& parsed, nlohmann::json* current) {
  size_t colon_pos = parsed.trimmed.find(':');
  if (colon_pos == std::string::npos) return;
  
  std::string key = parsed.trimmed.substr(0, colon_pos);
  std::string value = parsed.trimmed.substr(colon_pos + 1);
  
  while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
  value.erase(0, value.find_first_not_of(" \t"));
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
  
  if (value.empty()) {
    yaml_ctx_.pending_key = key;
    yaml_ctx_.pending_indent = parsed.indent;
    yaml_ctx_.pending_parent = current;
    return;
  }
  
  if (IsYamlMultilineIndicator(value)) {
    yaml_ctx_.in_multiline = true;
    yaml_ctx_.multiline_key = key;
    yaml_ctx_.multiline_indent = parsed.indent;
    yaml_ctx_.multiline_value = "";
    yaml_ctx_.multiline_fold = (value[0] == '>');
    return;
  }
  
  (*current)[key] = ParseYamlValue(value);
}

bool SkillParser::IsYamlMultilineIndicator(const std::string& value) {
  return value == ">" || value == "|" || value == ">-" || value == "|-" ||
         value == ">+" || value == "|+";
}

nlohmann::json SkillParser::ParseYamlValue(const std::string& value) {
  if (value.front() == '[' && value.back() == ']') {
    std::string inner = value.substr(1, value.size() - 2);
    nlohmann::json arr = nlohmann::json::array();
    std::istringstream items(inner);
    std::string item;
    while (std::getline(items, item, ',')) {
      item.erase(0, item.find_first_not_of(" \t\""));
      while (!item.empty() && (item.back() == ' ' || item.back() == '\t' || item.back() == '"')) {
        item.pop_back();
      }
      if (!item.empty()) arr.push_back(item);
    }
    return arr;
  }
  
  if (value == "true") return true;
  if (value == "false") return false;
  
  std::string str = value;
  if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
    str = str.substr(1, str.size() - 2);
  }
  return str;
}

void SkillParser::ResetYamlContext() {
  yaml_ctx_ = YamlParserContext();
  yaml_ctx_.ctx_stack.push({-1, &yaml_ctx_.root});
}

nlohmann::json SkillParser::ParseYamlFrontmatter(const std::string& yaml_str) {
  ResetYamlContext();
  
  std::istringstream stream(yaml_str);
  std::string line;
  
  while (std::getline(stream, line)) {
    ParsedLine parsed = ParseYamlLine(line);
    
    if (parsed.is_empty) continue;
    if (HandleYamlMultilineContinuation(parsed)) continue;
    
    UpdateYamlContextStack(parsed.indent);
    HandleYamlPendingKey(parsed);
    
    nlohmann::json* current = yaml_ctx_.ctx_stack.top().second;
    
    if (parsed.trimmed[0] == '-') {
      ParseYamlArrayItem(parsed);
      continue;
    }
    
    ParseYamlKeyValue(parsed, current);
  }
  
  FinalizeYamlPendingKey();
  FinalizeYamlMultilineString();
  
  return yaml_ctx_.root;
}

}  // namespace quantclaw
