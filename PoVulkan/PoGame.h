#pragma once

//#include <functional>
//#include <unordered_map>
#include <array>
// for now lets make a game that can pick between a cube and a sphere as its visuals
// the object can have it's transformation set, objects can be spawned
// objects can be deleted

struct SPoObjectId
{
	int mValue = -1;
	bool operator==(SPoObjectId const &) const = default;
	bool operator!=(SPoObjectId const &) const = default;
};
//
//template<>
//struct std::hash<SPoObjectId>
//{
//	std::size_t operator()(SPoObjectId const &obj) const noexcept
//	{
//		return obj.mValue;
//	}
//};

struct SPoComponentId
{
	int mValue = -1;
	bool operator==(SPoComponentId const &) const = default;
	bool operator!=(SPoComponentId const &) const = default;
};

//template<>
//struct std::hash<SPoComponentId>
//{
//	std::size_t operator()(SPoComponentId const &obj) const noexcept
//	{
//		return obj.mValue;
//	}
//};

enum class EPrimmitive : uint8_t
{
	Cube,
	Sphere,
	Count
};

struct SPoCamera
{

};

struct SPoGameShader
{

};

struct SPoGameMaterial
{

};

struct SPoCube
{

};

struct SPoSphere
{

};

struct SPoPointLight
{

};

struct SPoDirectionalLight
{

};


struct SPoScene
{
	//std::unordered_map<SPoObjectId, SPoPointLight> mPointLights;
	//std::unordered_map<SPoObjectId, SPoPointLight> mDirectionalLights;
	//std::unordered_map<SPoObjectId, SPoCube> mCubes;
	//std::unordered_map<SPoObjectId, SPoSphere> mSpheres;

	//std::unordered_map<SPoObjectId, SPoGameMaterial> mMaterials;

	//std::unordered_map<SPoObjectId, SPoCamera> mCameras;
};


struct SPoGameSettings
{

};

int const skMaxScenes = 20;

struct SPoGameState
{
	
	std::array<SPoScene, skMaxScenes> mScenes;

};

struct SPoGameResources
{

};

namespace NPoGameBehavior
{
	SPoGameState init(SPoGameResources &outResources, SPoGameSettings const &settings);
	
	void cleanup(SPoGameResources &inOutResources, SPoGameState &inOutState, SPoGameSettings const &settings);
}