// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include "quantclaw/skill/skill_install_deps.hpp"

#include <filesystem>

#include <spdlog/spdlog.h>

#include "quantclaw/platform/process.hpp"

namespace quantclaw {

SkillInstallDeps::SkillInstallDeps(
  std::shared_ptr<spdlog::logger> logger)
    : logger_(std::move(logger)) {
  full_loader_ = std::make_shared<SkillLoaderFull>(logger_); 
  logger_->info("SkillInstallDeps initialized");
}

// ---------------------------------------------------------------------------
// Skill dependency installation
// ---------------------------------------------------------------------------

bool SkillInstallDeps::InstallSkillDependence(
    const SkillFullData& skill) {
  // Delegate to SkillLoaderFull::CheckSkillGating first
  if (!full_loader_->CheckSkillGating(skill)) {
    logger_->debug("InstallSkillDependence: gating failed for '{}'",
                   skill.mini.name);
    return false;
  }

  if (skill.extra.installs.empty()) {
    logger_->info("Skill '{}' has no install instructions", skill.mini.name);
    return true;
  }

  bool all_ok = true;
  for (const auto& inst : skill.extra.installs) {
    std::string eff_binary = inst.EffectiveBinary();
    std::string eff_method = inst.EffectiveMethod();
    std::string eff_formula = inst.EffectiveFormula();

    // Skip if binary already available
    if (!eff_binary.empty() && IsBinaryAvailable(eff_binary)) {
      logger_->debug("Skill '{}': {} already installed", skill.mini.name,
                     eff_binary);
      continue;
    }

    std::string cmd;
    if (eff_method == "node") {
      cmd = "npm install -g " + eff_formula;
    } else if (eff_method == "go") {
      cmd = "go install " + eff_formula;
    } else if (eff_method == "uv") {
      cmd = "uv pip install " + eff_formula;
    } else if (eff_method == "apt") {
      cmd = "sudo apt-get install -y " + eff_formula;
    } else if (eff_method == "brew") {
      cmd = "brew install " + eff_formula;
    } else if (eff_method == "download") {
      std::string bin_dir = platform::home_directory() + "/.quantclaw/bin";
      std::filesystem::create_directories(bin_dir);
      std::string dest =
          bin_dir + "/" + (eff_binary.empty() ? "downloaded" : eff_binary);
      cmd =
          "curl -fsSL -o " + dest + " " + eff_formula + " && chmod +x " + dest;
    } else {
      logger_->warn("Skill '{}': unknown install method '{}'", skill.mini.name,
                    eff_method);
      continue;
    }

    logger_->info("Installing skill '{}' via {}: {}", skill.mini.name, eff_method,
                  cmd);
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
      logger_->error("Skill '{}' install failed (exit {})", skill.mini.name, ret);
      all_ok = false;
    }
  }
  return all_ok;
}

bool SkillInstallDeps::InstallSkillDependenceByName(
    const std::string& skill_name,
    const std::vector<SkillMetadata>& metas) {
  SkillMetadata target_meta;
  bool found = false;
  for (const auto& meta : metas) {
    if (meta.mini.name == skill_name) {
      target_meta = meta;
      found = true;
      break;
    }
  }

  if (!found) {
    logger_->warn("InstallSkillDependenceByName: skill '{}' not found in metas",
                  skill_name);
    return false;
  }

  // Load full data from the metadata, then install
  try {
    auto full_data = full_loader_->LoaderOneFullData(target_meta);
    return InstallSkillDependence(full_data);
  } catch (const std::exception& e) {
    logger_->error("InstallSkillDependenceByName: failed to load full data for '{}': {}",
                   skill_name, e.what());
    return false;
  }
}

int SkillInstallDeps::InstallAllSkillDependences(
    const std::vector<SkillMetadata>& metas) {
  int installed = 0;
  for (const auto& meta : metas) {
    try {
      auto full_data = full_loader_->LoaderOneFullData(meta);

      // Skip skills that don't need installation
      if (full_data.extra.installs.empty()) {
        continue;
      }

      // Check gating before attempting install
      if (!full_loader_->CheckSkillGating(full_data)) {
        logger_->debug("InstallAllSkillDependences: skipping '{}' (gating failed)",
                       full_data.mini.name);
        continue;
      }

      if (InstallSkillDependence(full_data)) {
        ++installed;
      }
    } catch (const std::exception& e) {
      logger_->error("InstallAllSkillDependences: failed for '{}': {}",
                     meta.mini.name, e.what());
    }
  }

  logger_->info("InstallAllSkillDependences: {}/{} skills installed", installed,
                metas.size());
  return installed;
}

bool SkillInstallDeps::IsBinaryAvailable(
    const std::string& binary_name) const {
#ifdef _WIN32
  std::string command = "where " + binary_name + " > nul 2>&1";
#else
  std::string command = "which " + binary_name + " > /dev/null 2>&1";
#endif
  int result = std::system(command.c_str());
  return result == 0;
}

}  // namespace quantclaw
