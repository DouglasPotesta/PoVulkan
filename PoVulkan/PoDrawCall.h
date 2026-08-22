#pragma once


struct SPoDarwCallSettings
{
	// render pass id
	// binding attributes
};


struct SPoDrawCallState
{

};


struct SPoDrawCallResources
{

};


namespace NPoDrawCallBehavior
{

	/**
	* okaty so this is going to be how game code requests draws from our graphics engine
	* so really these are draw call requestss
	* with a draw call request we will include everything that would be required to create it's ...
	* - renderpass
	* - beinding descriptions
	* - pipeline
	* the reasoning is that these will kind of act as a sort of id for which draw call set it should be added to
	* draw call sets will be a collection that reuses as much resources as possible
	* this complicatess renderpasses becasue we will want to be able to disambiguate renderpasses
	* for these reasons we will introduce the render graph concept where a renderpass is required to "id" its inputs and outputs along.
	* it will also be doubly linked with other renderpasses so that the handoff of these resources are easily identifiable.
	* 
	*/

	/*
	total reqs
	// logical device will be treated as a given constant don't need stuff required to recreate it
	// 
	(windowResources.mRenderPass, inOutResources.mLogicalDevice, state.mMsaaCount, state.mSurfaceFormat.format, state.mDepthFormat)
	windowResources.mDescriptorSetLayout (=> array of descriptor set layout binding)
	windowResources.mPipelineLayout, windowResources.mGraphicsPipeline, (=> state.mMsaaCount, state.mSlang, inOutResources.mSlang)
	
	*/

	/*
	
	hmmmmmmmmmmmm... renderpasses are quite intricate. I think I am going to want to define a bunch of explicit render pass archetypes
	those will have the actual renderpass resources, BUT we will then have po vulkan render passes which point to a render pass archetype
	and define the doubley linked components along with the strict inputs and output targets.
	e.g.
	archetype_settings
	{
		inputs : image_of_format_x, buffer_of_contents_with_some_layout
		outputs : imamge_of_format_y
		depthFormat = blah // these could be dynamic
		masaaSamples = k // these could be dynamic (ugh I realize why things blow up so much because they are making shaders for each possible format, msaasample etc)
		subpasses (=>pInputAttachments + outputAttachments (=> VkAttachmentDescriptions))
	}
	archetype_state
	{
		id => thing that identifies this amongst the existing archetypes
	}
	archetype_resources
	{
		renderpass
	}

	porenderpass
	{
		archetype_id
		pPreviousPoRenderPass
		pNextPoRenderPass
	}
	*/

}