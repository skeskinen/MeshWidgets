# MeshWidgets

Widgets on curved 3D surfaces or crazier things.

![Imgur](http://i.imgur.com/gx1zckw.gif)

Includes:

 * UMeshWidgetComponent, similar to UWidgetComponent but on static mesh surface
 * UMeshWidgetInteractionComponent, same as UWidgetInteractionComponent but works with mesh widgets and normal widget components

Requirements:

 * UE 4.20
 * UV hit testing project setting (Physics > Optimization > Support UV From Hit Results) ticked

Installation:

 * C++: Regenerate project files and add to your Build.cs
 * Blueprint only: Copy Plugins folder to your project. (Binary for drag&drop built for 4.20. USE AT OWN RISK generally don't run random binaries you got from the internet)
