@echo off
Setlocal EnableDelayedExpansion

for %%f in (*.vert) do (
	set "output_name=%%f"
	set "output_name=!output_name:.=_!"
	C:\VulkanSDK\1.4.350.0\Bin\glslc.exe %%f -o "!output_name!.spv"
)

for %%f in (*.frag) do (
	set "output_name=%%f"
	set "output_name=!output_name:.=_!"
	C:\VulkanSDK\1.4.350.0\Bin\glslc.exe %%f -o "!output_name!.spv"

)