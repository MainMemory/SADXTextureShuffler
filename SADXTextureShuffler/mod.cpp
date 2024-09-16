#include "pch.h"
#include "SADXModLoader.h"
#include "FunctionHook.h"
#include "IniFile.hpp"
#include <map>

int shuffleMode;
std::map<NJS_TEXLIST*, std::vector<Uint32>> loadedTextures;
std::vector<Uint32> textureBuffer;

void ShuffleTexlist(NJS_TEXLIST* texlist)
{
	for (Uint32 i = 0; i < texlist->nbTexture; ++i)
		std::swap(texlist->textures[i].texaddr, texlist->textures[rand() % texlist->nbTexture].texaddr);
}

void ShuffleAllTextures()
{
	size_t texcnt = 0;
	for (auto& kvp : loadedTextures)
	{
		if (texcnt + kvp.second.size() > textureBuffer.size())
			textureBuffer.resize(texcnt + kvp.second.size());
		memcpy(&textureBuffer[texcnt], kvp.second.data(), kvp.second.size() * sizeof(Uint32));
		texcnt += kvp.second.size();
	}
	textureBuffer.resize(texcnt);
	for (size_t i = 0; i < texcnt; ++i)
		std::swap(textureBuffer[i], textureBuffer[rand() % texcnt]);
	texcnt = 0;
	for (auto& kvp : loadedTextures)
	{
		for (size_t i = 0; i < kvp.first->nbTexture; ++i)
			kvp.first->textures[i].texaddr = textureBuffer[texcnt + i];
		texcnt += kvp.first->nbTexture;
	}
}

void AddTexlist(NJS_TEXLIST* texlist)
{
	if (shuffleMode)
	{
		std::vector<Uint32> texaddrs(texlist->nbTexture);
		for (size_t i = 0; i < texlist->nbTexture; ++i)
			texaddrs[i] = texlist->textures[i].texaddr;
		loadedTextures.insert({ texlist, texaddrs });
		ShuffleAllTextures();
	}
	else
		ShuffleTexlist(texlist);
}

FunctionHook<Sint32, NJS_TEXLIST*> njReleaseTexture__h(njReleaseTexture_);
Sint32 njReleaseTexture__r(NJS_TEXLIST* texlist)
{
	auto kvp = loadedTextures.find(texlist);
	if (kvp != loadedTextures.end())
	{
		for (size_t i = 0; i < texlist->nbTexture; ++i)
			texlist->textures[i].texaddr = kvp->second[i];
		loadedTextures.erase(kvp);
		ShuffleAllTextures();
	}
	return njReleaseTexture__h.Original(texlist);
}

FunctionHook<void, const char*, NJS_TEXLIST*> LoadPVM_h(LoadPVM);
void LoadPVM_r(const char* filename, NJS_TEXLIST* texlist)
{
	LoadPVM_h.Original(filename, texlist);
	AddTexlist(texlist);
}

FunctionHook<int, const char*, NJS_TEXLIST*> LoadPvmMEM2_h(LoadPvmMEM2);
int LoadPvmMEM2_r(const char* filename, NJS_TEXLIST* texlist)
{
	auto ret = LoadPvmMEM2_h.Original(filename, texlist);
	AddTexlist(texlist);
	return ret;
}

FunctionHook<Sint32, NJS_TEXLIST*> njLoadTexture_h(njLoadTexture);
Sint32 njLoadTexture_r(NJS_TEXLIST* texlist)
{
	auto ret = njLoadTexture_h.Original(texlist);
	AddTexlist(texlist);
	return ret;
}

extern "C"
{
	__declspec(dllexport) void Init(const char* path, const HelperFunctions& helperFunctions)
	{
		const IniFile* settings = new IniFile(std::string(path) + "\\config.ini");
		shuffleMode = settings->getInt("", "ShuffleMode", 0);
		delete settings;
		LoadPVM_h.Hook(LoadPVM_r);
		LoadPvmMEM2_h.Hook(LoadPvmMEM2_r);
		njLoadTexture_h.Hook(njLoadTexture_r);
		if (shuffleMode)
			njReleaseTexture__h.Hook(njReleaseTexture__r);
	}

	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer };
}