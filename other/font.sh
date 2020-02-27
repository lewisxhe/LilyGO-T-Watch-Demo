#!/bin/bash


# dataline=$(cat ./省份.txt)
dataline=$(cat ./地级市.txt)

# echo $dataline

lv_font_conv --font 方正粗黑宋简体.ttf --symbols $dataline --size 16 --format lvgl --bpp 1 -o ./chinese_font.c

# lv_font_conv --font 方正粗黑宋简体.ttf --symbols $dataline --size 21 --format lvgl --bpp 1 -o ./chinese_font.c


lv_font_conv --font digifaw.ttf --symbols "0123456789." --size 48 --format lvgl --bpp 1 -o ./digital_font.c
