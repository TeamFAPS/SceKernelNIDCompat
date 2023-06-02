# SceKernelNIDCompat

A kernel plugin to create compatibility to any kernel library

# What the this plugin method?

The PS Vita update version 3.63 changed some ForKernel NIDs and made many kernel plugins built with 3.60 NIDs incompatible with 3.63.

So the developer had to rewrite the kernel plugin for 3.63 again using the 3.63 NID.

However, by using this plugin, even kernel plugins created with 3.60 NID can be used with 3.63.

# Installation

1. Download nid_compat set from release download page. (Make sure you download this from the releases page)

2. Place nid_compat.skprx in any path and add that path to the KERNEL section of config.txt.

3. Copy nid_compat.txt to ux0:/data/nid_compat.txt and reboot.
