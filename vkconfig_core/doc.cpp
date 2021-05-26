/*
 * Copyright (c) 2020-2021 Valve Corporation
 * Copyright (c) 2020-2021 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors:
 * - Christophe Riccio <christophe@lunarg.com>
 */

#include "doc.h"

#include <QFileInfo>

static std::string GetProcessedDefaultValue(const SettingMeta& meta) {
    switch (meta.type) {
        case SETTING_GROUP: {
            return "";
        }
        case SETTING_LOAD_FILE:
        case SETTING_SAVE_FILE:
        case SETTING_SAVE_FOLDER: {
            return static_cast<const SettingMetaFilesystem&>(meta).default_value.c_str();
        }
        case SETTING_STRING:
        case SETTING_FRAMES: {
            return static_cast<const SettingMetaString&>(meta).default_value;
        }
        case SETTING_INT: {
            return format("%d", static_cast<const SettingMetaInt&>(meta).default_value);
        }
        case SETTING_FLOAT: {
            const SettingMetaFloat& meta_float = static_cast<const SettingMetaFloat&>(meta);
            const std::string float_format = meta_float.GetFloatFormat();

            return format(float_format.c_str(), meta_float.default_value).c_str();
        }
        case SETTING_BOOL_NUMERIC_DEPRECATED: {
            return static_cast<const SettingMetaBoolNumeric&>(meta).default_value ? "1" : "0";
        }
        case SETTING_BOOL: {
            return static_cast<const SettingMetaBool&>(meta).default_value ? "TRUE" : "FALSE";
        }
        case SETTING_LIST: {
            const SettingMetaList& meta_list = static_cast<const SettingMetaList&>(meta);

            std::string result;

            for (std::size_t i = 0, n = meta_list.default_value.size(); i < n; ++i) {
                if (!meta_list.default_value[i].enabled) continue;

                if (i != 0) {
                    result += ",";
                }
                if (meta_list.default_value[i].key.empty()) {
                    result += meta_list.default_value[i].number;
                } else {
                    result += meta_list.default_value[i].key.c_str();
                }
            }

            return result;
        }
        case SETTING_ENUM: {
            return static_cast<const SettingMetaEnum&>(meta).default_value;
        }
        case SETTING_FLAGS: {
            const SettingMetaFlags& meta_list = static_cast<const SettingMetaFlags&>(meta);

            std::string result;

            for (std::size_t i = 0, n = meta_list.default_value.size(); i < n; ++i) {
                result += meta_list.default_value[i].c_str();
                if (i < n - 1) {
                    result += ",";
                }
            }

            return result;
        }
        default: {
            assert(0);
            return "";
        }
    }
}

static std::string GetProcessedValue(const Layer& layer, const SettingData& data) {
    switch (data.type) {
        case SETTING_GROUP: {
            return "";
        }
        case SETTING_LOAD_FILE:
        case SETTING_SAVE_FILE:
        case SETTING_SAVE_FOLDER: {
            return static_cast<const SettingDataString&>(data).value.c_str();
        }
        case SETTING_FRAMES: {
            return static_cast<const SettingDataFrames&>(data).value;
        }
        case SETTING_STRING:
        case SETTING_ENUM: {
            return static_cast<const SettingDataString&>(data).value;
        }
        case SETTING_INT: {
            return format("%d", static_cast<const SettingDataInt&>(data).value);
        }
        case SETTING_FLOAT: {
            const SettingDataFloat& setting_data = static_cast<const SettingDataFloat&>(data);
            const SettingMetaFloat* meta = FindSettingMeta<SettingMetaFloat>(layer.settings, data.key.c_str());

            const std::string float_format = meta->GetFloatFormat();

            if (meta->IsValid(setting_data)) {
                return format(float_format.c_str(), setting_data.value).c_str();
            } else {
                return format(float_format.c_str(), meta->default_value).c_str();
            }
        }
        case SETTING_BOOL_NUMERIC_DEPRECATED: {
            return static_cast<const SettingDataBool&>(data).value ? "1" : "0";
        }
        case SETTING_BOOL: {
            return static_cast<const SettingDataBool&>(data).value ? "TRUE" : "FALSE";
        }
        case SETTING_LIST: {
            const SettingDataList& data_list = static_cast<const SettingDataList&>(data);

            std::string result;

            for (std::size_t i = 0, n = data_list.value.size(); i < n; ++i) {
                if (!data_list.value[i].enabled) continue;

                if (i != 0) {
                    result += ",";
                }
                if (data_list.value[i].key.empty()) {
                    result += data_list.value[i].number;
                } else {
                    result += data_list.value[i].key.c_str();
                }
            }

            return result;
        }
        case SETTING_FLAGS: {
            const SettingDataFlags& data_list = static_cast<const SettingDataFlags&>(data);

            std::string result;

            for (std::size_t i = 0, n = data_list.value.size(); i < n; ++i) {
                result += data_list.value[i].c_str();
                if (i < n - 1) {
                    result += ",";
                }
            }

            return result;
        }
        default: {
            assert(0);
            return "";
        }
    }
}

static std::string BuildPlatformsHTML(int platform_flags) {
    std::string text;

    const std::vector<std::string>& platforms = GetPlatformTokens(platform_flags);
    for (std::size_t i = 0, n = platforms.size(); i < n; ++i) {
        text += "<span class=\"code\">" + platforms[i] + "</ span>";
        if (i < n - 1) {
            text += ", ";
        }
    }

    return text;
}

static void WriteSettingsOverview(std::string& text, const Layer& layer, const SettingMetaSet& settings) {
    for (std::size_t i = 0, n = settings.Size(); i < n; ++i) {
        if (settings[i].type != SETTING_GROUP && settings[i].view != SETTING_VIEW_HIDDEN) {
            text += "<tr>\n";
            text += format("\t<td><a id=\"%s\"href=\"#%s-detailed\">%s</a></td>\n", settings[i].key.c_str(),
                           settings[i].key.c_str(), settings[i].label.c_str());

            text += format("\t<td><span class=\"code\">%s</span></td>\n", GetSettingTypeToken(settings[i].type));
            text += format("\t<td><span class=\"code\">%s</span></td>\n", GetProcessedDefaultValue(settings[i]).c_str());

            text +=
                format("\t<td><span class=\"code\">%s</span></td>\n", (GetLayerSettingPrefix(layer.key) + settings[i].key).c_str());
            if (settings[i].env.empty()) {
                text += format("\t<td>N/A</td>\n", settings[i].env.c_str());
            } else {
                text += format("\t<td><span class=\"code\">%s</span></td>\n", settings[i].env.c_str());
            }

            text += format("\t<td>%s</td>\n", BuildPlatformsHTML(settings[i].platform_flags).c_str());
            text += "</tr>\n";
        }

        WriteSettingsOverview(text, layer, settings[i].children);
    }
}

static const std::string GetLayerSettingsDocURL(const Layer& layer) {
    if (layer.api_version > Version(1, 7, 176)) {
        return format("https://github.com/LunarG/VulkanTools/tree/sdk-%s.0/vkconfig#vulkan-layers-settings",
                      layer.api_version.str().c_str());
    } else {
        return "https://github.com/LunarG/VulkanTools/tree/master/vkconfig#vulkan-layers-settings";
    }
}

static void WriteSettingsDetails(std::string& text, const Layer& layer, const SettingMetaSet& settings) {
    for (std::size_t i = 0, n = settings.Size(); i < n; ++i) {
        if (settings[i].type != SETTING_GROUP && settings[i].view != SETTING_VIEW_HIDDEN) {
            if (settings[i].status == STATUS_STABLE) {
                text += format("<h3><a id=\"%s-detailed\" href=\"#%s\">%s</a></h3>\n", settings[i].key.c_str(),
                               settings[i].key.c_str(), settings[i].label.c_str());
            } else {
                text += format("<h3><a id=\"%s-detailed\" href=\"#%s\">%s</a> (%s)</h3>\n", settings[i].key.c_str(),
                               settings[i].key.c_str(), settings[i].label.c_str(), GetToken(settings[i].status));
            }

            text += format("\t<p>%s</p>\n", settings[i].description.c_str());

            text += "<h4>Setting Properties:</h4>\n";
            text += "<ul>\n";
            text += format("\t<li><a href=\"%s\">vk_layer_settings.txt</a> Variable: <span class=\"code\">%s</span></li>\n",
                           GetLayerSettingsDocURL(layer).c_str(), (GetLayerSettingPrefix(layer.key) + settings[i].key).c_str());
            if (settings[i].env.empty()) {
                text += format("\t<li>Environment Variable: <span class=\"code\">%s</span></li>\n", "N/A");
            } else {
                text += format("\t<li>Environment Variable: <span class=\"code\">%s</span></li>\n", settings[i].env.c_str());
            }
            text += format("\t<li>Platforms Supported: %s</li>\n", BuildPlatformsHTML(settings[i].platform_flags).c_str());

            if (settings[i].view != SETTING_VIEW_STANDARD) {
                text += format("\t<li>Setting Level: %s</li>\n", GetSettingViewToken(settings[i].view));
            }

            text += "</ul>\n";

            text += format(
                "\t<p>Setting Type: <span class=\"code\">%s</span> - Setting Default Value: <span class=\"code\">%s</span></p>\n",
                GetSettingTypeToken(settings[i].type), GetProcessedDefaultValue(settings[i]).c_str());

            if (IsEnum(settings[i].type)) {
                const SettingMetaEnumeration& setting_enum = static_cast<const SettingMetaEnumeration&>(settings[i]);

                text += "<table>\n";
                text +=
                    "<thead><tr><th>Enum Value</th><th>Label</th><th class=\"desc\">Description</th><th>Platforms "
                    "Supported</th></tr></thead>\n";
                text += "<tbody>\n";
                for (std::size_t j = 0, o = setting_enum.enum_values.size(); j < o; ++j) {
                    const SettingEnumValue& value = setting_enum.enum_values[j];

                    if (value.view == SETTING_VIEW_HIDDEN) continue;

                    text += "<tr>\n";
                    text += format("\t<td>%s</td>\n", value.key.c_str());
                    text += format("\t<td>%s</td>\n", value.label.c_str());
                    if (value.description.empty()) {
                        text += "\t<td>N/A</td>\n";
                    } else {
                        text += format("\t<td class=\"desc\">%s</td>\n", value.description.c_str());
                    }
                    text += format("\t<td>%s</td>\n", BuildPlatformsHTML(settings[i].platform_flags).c_str());
                    text += "</tr>\n";
                }
                text += "</tbody></table>\n";
            }
        }

        WriteSettingsDetails(text, layer, settings[i].children);
    }
}

void ExportHtmlDoc(const Layer& layer, const std::string& path) {
    std::string text;

    text += "<!DOCTYPE html>\n";
    text += "<html>\n";
    text += format("<head><title></title></head>\n", layer.key.c_str());
    text += "<body>\n";
    text += "<style>\n";
    text += "\ta {color: #A41E22;}\n";
    text += "\th1 {color: #A41E22;}\n";
    text += "\th2 {color: #A41E22;}\n";
    text += "\ttable {border: 1px solid; width: 100%; margin-left: auto; margin-right: auto;}\n";
    text += "\ttd {border: 1px dotted;}\n";
    text += "\t.code {color: #008000; font-family: consolas; }\n";
    text += "\t.desc {width:50%;}\n";
    text += "</style>\n";

    text += "<h1 id=\"top\">";
    if (layer.url.empty()) {
        text += format("%s\n", layer.key.c_str());
    } else {
        text += format("<a href=\"%s\">%s</a>\n", layer.url.c_str(), layer.key.c_str());
    }

    if (layer.status != STATUS_STABLE) {
        text += format(" (%s)", GetToken(layer.status));
    }
    text += "</h1>\n";

    if (!layer.description.empty()) {
        text += "<h3>" + layer.description + "</h3>\n";
    }

    if (!layer.introduction.empty()) {
        text += format("<p>%s</p>\n", layer.introduction.c_str());
    }

    text += "<h2><a href=\"#top\">Layer Properties</a></h2>\n";
    text += "<ul>\n";
    text += format("\t<li>API Version: %s</li>\n", layer.api_version.str().c_str());
    text += format("\t<li>Implementation Version: %s</li>\n", layer.implementation_version.c_str());
    text += format("\t<li>Layer Manifest: %s<ul>\n", QFileInfo(layer.manifest_path.c_str()).fileName().toStdString().c_str());
    text += format("\t\t<li>File Format: %s</li>\n", layer.file_format_version.str().c_str());
    text += format("\t\t<li>Layer Binary Path: %s</li>\n", layer.binary_path.c_str());
    text += "\t</ul></li>\n";
    if (layer.platforms != 0) {
        text += format("\t<li>Supported Platforms: %s</li>\n", BuildPlatformsHTML(layer.platforms).c_str());
    }
    if (layer.status != STATUS_STABLE) {
        text += format("\t<li>Status: %s</li>\n", GetToken(layer.status));
    }
    if (!layer.settings.Empty()) {
        text += format("\t<li><a href=\"#settings\">Number of Layer Settings: %d</a></li>\n", layer.settings.Size());
    }
    if (!layer.presets.empty()) {
        text += format("\t<li><a href=\"#presets\">Number of Layer Presets: %d</a></li>\n", layer.presets.size());
    }
    text += "</ul>\n";

    if (!layer.url.empty()) {
        text +=
            format("<p>Visit <a href=\"%s\">%s home page</a> for more information.</p>\n", layer.url.c_str(), layer.key.c_str());
    }

    if (!layer.settings.Empty()) {
        text += "<h2><a href=\"#top\" id=\"settings\">Layer Settings Overview</a></h2>\n";
        text += "<table><thead><tr>";
        text += format(
            "<th>Setting</th><th>Type</th><th>Default Value</th><th><a href=\"%s\">vk_layer_settings.txt</a> Variable</th>"
            "<th>Environment Variable</th><th>Supported Platforms</th>",
            GetLayerSettingsDocURL(layer).c_str());
        text += "</tr></thead><tbody>\n";
        WriteSettingsOverview(text, layer, layer.settings);
        text += "</tbody></table>\n";

        text += "<h2><a href=\"#top\">Layer Settings Details</a></h2>\n";
        WriteSettingsDetails(text, layer, layer.settings);
    }

    if (!layer.presets.empty()) {
        text += "<h2><a href=\"#top\" id=\"presets\">Layer Presets</a></h2>\n";
        for (std::size_t i = 0, n = layer.presets.size(); i < n; ++i) {
            const LayerPreset& preset = layer.presets[i];

            text += format("<h3>%s</h3>\n", preset.label.c_str());
            text += format("<p>%s</p>", preset.description.c_str());

            text += "<h4>Preset Setting Values:</h4>\n";
            text += "<ul>\n";

            for (std::size_t j = 0, o = preset.settings.Size(); j < o; ++j) {
                const SettingData& data = preset.settings[j];
                const SettingMeta* meta = FindSettingMeta(layer.settings, data.key.c_str());

                text += format("\t<li><a href=\"#%s-detailed\">%s</a>: <span class=\"code\">%s</span></li>\n", meta->key.c_str(),
                               meta->label.c_str(), GetProcessedValue(layer, data).c_str());
            }

            text += "</ul>\n";
        }
    }

    text += "</body>\n";
    text += "</html>\n";

    QFile file(path.c_str());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(text.c_str());
        file.close();
    }
}

void ExportSettingsDoc(const Layer& layer, const std::string& path) {}
