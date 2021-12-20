# Dart-Plotter
This projec will detect holes in a sheet display and plot them. Optinally you can save the results to a file.

# How does it work?

It works using an object recognition system that can recognize multiple objects in a single frame called yolov4.

I have trained my own database using darknet(https://github.com/AlexeyAB/darknet) 

The images were annotated by myself using roboflow (https://app.roboflow.com/).

The GUI is using a personally modified version of dear ImGui (https://github.com/ocornut/imgui)
