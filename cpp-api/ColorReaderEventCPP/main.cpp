// This file is part of the Orbbec Astra SDK [https://orbbec3d.com]
// Copyright (c) 2015-2017 Orbbec 3D
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Be excellent to each other.
#include <astra/astra.hpp>
#include <cstdio>
#include <iostream>
#include <key_handler.h>
int bluecount = 0;
int bluebool = false;
int blueposbuffer = 0;
int initbuffer = 0;

class SampleFrameListener : public astra::FrameListener
{
private:
    using buffer_ptr = std::unique_ptr<astra::RgbPixel[]>;
    buffer_ptr buffer_;
    unsigned int lastWidth_;
    unsigned int lastHeight_;
public:
    virtual void on_frame_ready(astra::StreamReader& reader,
        astra::Frame& frame) override
    {
        const astra::ColorFrame colorFrame = frame.get<astra::ColorFrame>();
        if (colorFrame.is_valid())
        {
            print_color(colorFrame);
        }
    }
    void print_color(const astra::ColorFrame& colorFrame)
    {
        if (colorFrame.is_valid())
        {
            int width = colorFrame.width();
            int height = colorFrame.height();
            int frameIndex = colorFrame.frame_index();
            if (width != lastWidth_ || height != lastHeight_) {
                buffer_ = buffer_ptr(new astra::RgbPixel[colorFrame.length()]);
                lastWidth_ = width;
                lastHeight_ = height;
            }
            colorFrame.copy_to(buffer_.get());
            size_t index = ((width * (height / 2.0f)) + (width / 2.0f));
            astra::RgbPixel middle = buffer_[index];
            //std::cout << "color frameIndex: " << frameIndex
            //    << " r: " << static_cast<int>(middle.r)
            //    << " g: " << static_cast<int>(middle.g)
            //    << " b: " << static_cast<int>(middle.b)
            //    << std::endl;
            //initbuffer++
            initbuffer++;
            //std::cout << " initbuffer " << initbuffer;
            bool ispastinit = initbuffer >= 40;
            bool isbelt = static_cast<int>(middle.r) < 30 && static_cast<int>(middle.g) < 30 && static_cast<int>(middle.b) < 30;
            //bool isbuckle = static_cast<int>(middle.r) > 100  && static_cast<int>(middle.g) > 100 && static_cast<int>(middle.b) > 100;

            bool isblue = static_cast<int>(middle.r) < 50 && static_cast<int>(middle.b) > 200;
            if (!isblue) {
                bluebool = false;
                blueposbuffer = 0;
            }
            if (!isbelt && ispastinit) {

                std::cout << " Something on the belt! ";
                //Blue
                if (isblue) {
                    std::cout << " This is Blue!";
                    bluebool = true;
                    blueposbuffer++;
                }
                else {
                    bluebool = false;
                    blueposbuffer = 0;
                }
                if (blueposbuffer == 1) {
                    bluecount++;
                }
                //std::cout << " Bluebool: " << bluebool;
                std::cout << " Bluecount: " << bluecount;
                //std::cout << " Blueposbuffer: " << blueposbuffer;
                //Red
                if (static_cast<int>(middle.r) > 200 && static_cast<int>(middle.b) < 80) {
                    std::cout << " This is Red!";
                }

                //Not  Purple
                bool ispurple = static_cast<int>(middle.r) > 125 && static_cast<int>(middle.g) < 75 && static_cast<int>(middle.b) > 150;
                if (!ispurple) {
                    std::cout << " This is NOT Purple!";
                }
                std::cout << std::endl;
                std::cout << std::endl;
            }

        }
    }
};
int main(int argc, char** argv)
{
    astra::initialize();
    set_key_handler();
    astra::StreamSet streamSet;
    astra::StreamReader reader = streamSet.create_reader();
    SampleFrameListener listener;
    reader.stream<astra::ColorStream>().start();
    std::cout << "colorStream -- hFov: "
        << reader.stream<astra::ColorStream>().hFov()
        << " vFov: "
        << reader.stream<astra::ColorStream>().vFov()
        << std::endl;
    reader.add_listener(listener);
    do
    {
        astra_update();
    } while (shouldContinue);
    reader.remove_listener(listener);
    astra::terminate();
}