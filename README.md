# ResourcePackFix

A private-use plugin for our Bedrock Dedicated Server (Windows x64), built on
LeviLamina. The plugin source is not published; this repository exists only to
document the mod for an MCAPI symbol request.

## What it does

Hooks `ResourcePackFileUploadManager::_addZipPath` at function entry and
redirects the registered resource pack ZIP path to the mod's cache directory,
so packs registered at runtime are served from a stable local path.

## Environment

- LeviLamina 26.60.0 (server)
- bedrock-runtime-data v26.60.6-server.5
- prelink v0.8.8

## Why the symbol is needed

In 26.51 headers `_addZipPath` was `MCAPI`; in 26.60.0 it became `MCNAPI`, so
after prelinking the import symbol is missing and the hook fails to link with
LNK2019. Only this single function is requested, not the whole class.

```cpp
void ResourcePackFileUploadManager::_addZipPath(
    Core::PathBuffer<std::string> const& resourceZipPath
);
```
