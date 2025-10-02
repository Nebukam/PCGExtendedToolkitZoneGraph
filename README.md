![UE 5.6](https://img.shields.io/badge/UE-5.6-darkgreen) ![5.5](https://img.shields.io/badge/5.5-darkgreen) [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Nebukam/PCGExtendedToolkitZoneGraph)
# PCG Extended Toolkit + Zone Graph

![PCGEx](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/refs/heads/docs/_sources/smol-logo.png)

## Purpose of Plugin

This **experimental** ZoneGraph plugin allows [PCG Extended Toolkit](https://nebukan.github.io/PCGExtendedToolkit) data to be an input source for the [ZoneGraph Plugin]() (also currently **experimental**) from Epic Games.

PCGExtendedToolkitZoneGraph provides a single PCG Element node, PCGExClusterToZoneGraph, which executes on the main thread and converts a Cluster from PCGEx to a ZoneGraph for the Epic Games plugin.

## EXPERIMENTAL Status

Both PCGExtendedToolkitZoneGraph (this plugin) and the ZoneGraph plugin on which it relies are **experimental** code. Epic Games warns "ZoneGraph is currently an experimental plugin that will have API breaking changes as its development progresses towards a full release in later engine versions." PCGExtendedToolkit is released but is under active development as well. Given these factors, PCGExtendedToolkitZoneGraph should be approached carefully and is not recommended for shipping products.

## Getting Started

Begin by installing PCGExtendedToolkit and ZoneGraph plugins (order does not matter) and restarting the engine once both are enabled. Refer to the documentation for the respective plugins, listed below, for more detailed installation instructions.

Once the two prerequisite plugins are enabled and the Unreal Editor restarts without problems, add PCGExtendedToolkitZoneGraph plugin to its own subdirectory under the project or the engine Plugins directory, and compile the plugin if required. Enable PCGExtendedToolkitZoneGraph in your Project Settings, and restart the engine once more.

- **[PCG Extended Toolkit Documentation](https://pcgex.gitbook.io/pcgex)**  
- **[PCG Ex Installation](https://nebukam.github.io/PCGExtendedToolkit/installation.html) in your own project**
- **[ZoneGraph Quick Start Guide](https://dev.epicgames.com/community/learning/tutorials/qz6r/unreal-engine-zonegraph-quick-start-guide)**


For questions & support, join the [PCGEx Discord Server](https://discord.gg/mde2vC5gbE)!

