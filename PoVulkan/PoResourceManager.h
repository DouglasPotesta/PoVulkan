#pragma once

#include "PoMesh.h"
#include "PoTexture.h"

#include <vector>


/*
* The goal for this is to be able to keep track of dynamically allocated resources.
* The resources will be given ids that are used to invoke commands.
* That way game code isn't directly loading resources.
* For example : 
* SPoBullet::Tick(SPoGammeObjectState &inOutState, SPoWorld &world{SPoResourceManagerState &inOutResourceState})
*	inOutState.mModelInstance = SPoWorld::SpawnModelInstance(world, modelId);...
* where SpawnModel would return a dummy instance while its loading if it wanted asynchronous loading
* or it returns an immediately reaady one since it may have it already loaded from before
*	
*/

// TODO : Add a new header file for hosuing the shader resource

struct SPoShaderResources
{

};

struct SPoResourceManagerSettings
{

};

struct SPoResourceManagerState
{

};

struct SPoResourceManagerResources
{
	std::vector<SPoMeshResources> mMeshes;
	std::vector<SPoTextureResources> mTextures;
	std::vector<SPoShaderResources> mShaders;
};

namespace NPoResourceManagerBehavior
{

}