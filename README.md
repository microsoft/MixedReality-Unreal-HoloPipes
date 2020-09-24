# HoloPipes

Play the classic pipes puzzle game, taken to another dimension! HoloPipes is an open source sample app for HoloLens 2, built with Unreal Engine. If you're just looking to try out the app, HoloPipes is also available for download to a HoloLens 2 device from the [Microsoft Store](https://www.microsoft.com/en-us/p/holopipes/9mszb3nnrxn9). 

Supported Unreal versions | Supported device
:-----------------: | :----------------:
Unreal Engine 4.25+ | HoloLens 2


![Placing a pipe in the puzzle](docs/PlacePipe.png)

## How to run the app

1. Navigate to the **scripts** folder and run the **Build-SGM.ps1** powershell script. Wait until you've received confirmation that all the dependencies have been successfully downloaded. 
   * HoloPipes uses [WinRT APIs](https://docs.microsoft.com/en-us/windows/mixed-reality/develop/unreal/unreal-winrt) to save the game state (completed levels, stars, etc.) to device. This allows the app to keep track of where you left off. Since Unreal doesn't natively compile WinRT code, this script will automatically pull in the required NuGet libraries and build a separate binary that can be consumed by Unreal's build system. 


2. Navigate to the **pipes** folder and open up **HoloPipes.uproject** in Unreal Engine 4.25 or later. 

   * To stream the experience from a PC to a HoloLens 2 headset, follow the instructions for [streaming in Unreal](https://docs.microsoft.com/en-us/windows/mixed-reality/unreal-streaming).

   * To package and deploy the app to a HoloLens 2 headset, follow the instructions for [deploying to device in Unreal](https://docs.microsoft.com/en-us/windows/mixed-reality/unreal-deploying).

![Scrolling menu with unlimited levels](docs/Menu.png)

# Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
