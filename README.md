#  DepthCalc - Qt/C++ application for multi‑format depth data processing and plotting
---
This project is an application software for processing and interpreting measurement complex data, including algorithms for calibrating, filtering and converting signals, as well as generating output files for further analysis.

## Features
- Multi-format import and conversion: a separate module for downloading and converting PRZ/IFH/DVL with filtering and resampling.
- Optimized rendering of QCustomPlot graphs : load reduction due to visibility windows and data preloading.
- Multithreading: background operations (snapshots, downloads) are transferred to worker threads, there is progress and cancellation.
- Optimized work with large volumes of files.
- Modular architecture: domain managers (conversion, calibration, snapshots).
- An Undo/Redo system with stateful data and metadata.

## Links
- Video demonstration on YouTube: https://youtu.be/1fQBfJXa9o0

## Credits

#### Developer
Andrei Pereverzin
#### Software
- Qt Creator
- VS Code
