import os

filepath = 'src/pages/settings/functionSettings/components/cloudTranslationEngineManager.tsx'

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Fix flushConfig call (line 88-93)
old1 = '\t\tawait cloudTranslateSetConfig(updated).catch(() => {\n\t\t\tmessage.error("保存翻译引擎配置失败");\n\t\t});\n\t\tupdateAppSettings(\n\t\t\tAppSettingsGroup.FunctionTranslation,\n\t\t\t{ cloudTranslationConfig: updated },\n\t\t\ttrue,\n\t\t\ttrue,\n\t\t);'
new1 = '\t\tawait cloudTranslateSetConfig(updated).catch(() => {\n\t\t\tmessage.error("保存翻译引擎配置失败");\n\t\t});\n\t\tupdateAppSettings(\n\t\t\tAppSettingsGroup.FunctionTranslation,\n\t\t\t{ cloudTranslationConfig: updated },\n\t\t\ttrue,\n\t\t\ttrue,\n\t\t\tfalse,\n\t\t);'
content = content.replace(old1, new1)

# Fix moveUp call (line 106-111)
old2 = '\t\t\tcloudTranslateSetConfig(updated).catch(() => {});\n\t\t\tupdateAppSettings(\n\t\t\t\tAppSettingsGroup.FunctionTranslation,\n\t\t\t\t{ cloudTranslationConfig: updated },\n\t\t\t\ttrue,\n\t\t\t\ttrue,\n\t\t\t);\n\t\t},\n\t\t[updateAppSettings],\n\t);\n\n\t// Move engine down'
new2 = '\t\t\tcloudTranslateSetConfig(updated).catch(() => {});\n\t\t\tupdateAppSettings(\n\t\t\t\tAppSettingsGroup.FunctionTranslation,\n\t\t\t\t{ cloudTranslationConfig: updated },\n\t\t\t\ttrue,\n\t\t\t\ttrue,\n\t\t\t\tfalse,\n\t\t\t);\n\t\t},\n\t\t[updateAppSettings],\n\t);\n\n\t// Move engine down'
content = content.replace(old2, new2)

# Fix moveDown call (line 126-131)
old3 = '\t\t\tcloudTranslateSetConfig(updated).catch(() => {});\n\t\t\tupdateAppSettings(\n\t\t\t\tAppSettingsGroup.FunctionTranslation,\n\t\t\t\t{ cloudTranslationConfig: updated },\n\t\t\t\ttrue,\n\t\t\t\ttrue,\n\t\t\t);\n\t\t},\n\t\t[updateAppSettings],\n\t);\n\n\t// Update API key config'
new3 = '\t\t\tcloudTranslateSetConfig(updated).catch(() => {});\n\t\t\tupdateAppSettings(\n\t\t\t\tAppSettingsGroup.FunctionTranslation,\n\t\t\t\t{ cloudTranslationConfig: updated },\n\t\t\t\ttrue,\n\t\t\t\ttrue,\n\t\t\t\tfalse,\n\t\t\t);\n\t\t},\n\t\t[updateAppSettings],\n\t);\n\n\t// Update API key config'
content = content.replace(old3, new3)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)

print('Fixed all 3 updateAppSettings calls')
