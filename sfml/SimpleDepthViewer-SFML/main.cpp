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
#include <SFML/Graphics.hpp>
#include <astra/astra.hpp>
#include "LitDepthVisualizer.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <key_handler.h>
#include <sstream>


class DepthFrameListener : public astra::FrameListener
{
public:

    bool shotsfired = false;
    int maxX = 0;
    int maxY = 0;
    int minX = 100000;
    int minY = 100000;

    int realx = 0;
    int realy = 0;
    int realz = 0;

    float zvalue = 0;
    float xvalue = 0;
    float yvalue = 0;

    int carlength = 240;
    int widthmin = 230;
    int widthmax = 270;
    int depthmin = 1205;
    int depthmax = 1245;

    int n = 2; //number of frames averaged

    int flag = 0;
    
    samples::common::LitDepthVisualizer visualizer_;

    using DurationType = std::chrono::milliseconds;
    using ClockType = std::chrono::high_resolution_clock;

    ClockType::time_point prev_;
    float elapsedMillis_{ .0f };

    sf::Texture texture_;
    sf::Sprite sprite_;
    sf::Font font_;

    int displayWidth_{ 0 };
    int displayHeight_{ 0 };

    using BufferPtr = std::unique_ptr<uint8_t[]>;
    BufferPtr displayBuffer_{ nullptr };

    int depthWidth_{ 0 };
    int depthHeight_{ 0 };

    using DepthPtr = std::unique_ptr<int16_t[]>;
    DepthPtr depthData_{ nullptr };

    int mouseX_{ 0 };
    int mouseY_{ 0 };
    float mouseWorldX_{ 0 };
    float mouseWorldY_{ 0 };
    float mouseWorldZ_{ 0 };
    bool isPaused_{ false };
    bool isMouseOverlayEnabled_{ true };

    DepthFrameListener()
    {
        prev_ = ClockType::now();
        font_.loadFromFile("Inconsolata.otf");
    }

    void screen_shotwindow(sf::RenderWindow& render_window, std::string filename) {
        sf::Texture texture;
        texture.create(render_window.getSize().x, render_window.getSize().y);
        texture.update(render_window);

        if (texture.copyToImage().saveToFile(filename)) {
            std::cout << "Took screenshot : " << filename << std::endl;
            isPaused_ = true;
        }
    }

    void init_texture(int width, int height)
    {
        if (!displayBuffer_ ||
            width != displayWidth_ ||
            height != displayHeight_)
        {
            displayWidth_ = width;
            displayHeight_ = height;

            // texture is RGBA
            const int byteLength = displayWidth_ * displayHeight_ * 4;

            displayBuffer_ = BufferPtr(new uint8_t[byteLength]);
            std::fill(&displayBuffer_[0], &displayBuffer_[0] + byteLength, 0);

            texture_.create(displayWidth_, displayHeight_);
            sprite_.setTexture(texture_, true);
            sprite_.setPosition(0, 0);
        }
    }

    void check_fps()
    {
        const float frameWeight = .2f;

        const ClockType::time_point now = ClockType::now();
        const float elapsedMillis = std::chrono::duration_cast<DurationType>(now - prev_).count();

        elapsedMillis_ = elapsedMillis * frameWeight + elapsedMillis_ * (1.f - frameWeight);
        prev_ = now;

        const float fps = 1000.f / elapsedMillis;

        const auto precision = std::cout.precision();
        /*
                std::cout << std::fixed
                          << std::setprecision(1)
                          << fps << " fps ("
                          << std::setprecision(1)
                          << elapsedMillis_ << " ms)"
                          << std::setprecision(precision)
                          << std::endl; 
        */
    }

    void on_frame_ready(astra::StreamReader& reader,
        astra::Frame& frame) override
    {
        const astra::PointFrame pointFrame = frame.get<astra::PointFrame>();
        const int width = pointFrame.width();
        const int height = pointFrame.height();

        init_texture(width, height);

        check_fps();

        if (isPaused_) { return; }

        copy_depth_data(frame);

        visualizer_.update(pointFrame);

        const astra::RgbPixel* vizBuffer = visualizer_.get_output();

        if (flag < n - 1) //to take the average of 2 frames (hopefully to remove jittering)
        {
            flag++;
            for (int i = 0; i < width * height; i++)
            {
                const int rgbaOffset = i * 4;
                if (depthData_[i] > depthmin && depthData_[i] < depthmax && ((i / width) > widthmin) && ((i / width) < widthmax))
                {
                    displayBuffer_[rgbaOffset] += (vizBuffer[i].r) / n;
                    displayBuffer_[rgbaOffset + 1] += (vizBuffer[i].b) / n;
                    displayBuffer_[rgbaOffset + 2] += (vizBuffer[i].g) / n;
                    displayBuffer_[rgbaOffset + 3] = 255; //keep dont change
                }
                else
                {
                    displayBuffer_[rgbaOffset] = 0;
                    displayBuffer_[rgbaOffset + 1] = 0;
                    displayBuffer_[rgbaOffset + 2] = 0;
                    displayBuffer_[rgbaOffset + 3] = 255; //keep dont change
                }
            }
            return;
        }
        else if (flag == n - 1)
        {
            for (int i = 0; i < width * height; i++)
            {
                const int rgbaOffset = i * 4;

                displayBuffer_[rgbaOffset] += (vizBuffer[i].r) / n;
                displayBuffer_[rgbaOffset + 1] += (vizBuffer[i].b) / n;
                displayBuffer_[rgbaOffset + 2] += (vizBuffer[i].g) / n;
                displayBuffer_[rgbaOffset + 3] = 255; //keep dont change

                int row = i / width;
                int col = i % width;

                if (row > maxY) { maxY = row; }
                else if (row < minY) { minY = row; }
                if (col > maxX) { maxX = col; }
                else if (col < minX) { minX = col; }

                if (((maxX - minX)) > carlength)
                {
                    widthmin--;
                    widthmax--;
                    depthmin--;
                }
                else
                {
                    widthmin = 230;
                    widthmax = 270;
                    depthmin = 1205;
                    depthmax = 1245;
                }
            }
            flag = 0;
        }
        texture_.update(displayBuffer_.get());
    }

    void copy_depth_data(astra::Frame& frame)
    {
        const astra::DepthFrame depthFrame = frame.get<astra::DepthFrame>();

        if (depthFrame.is_valid())
        {
            const int width = depthFrame.width();
            const int height = depthFrame.height();

            if (!depthData_ || width != depthWidth_ || height != depthHeight_)
            {
                depthWidth_ = width;
                depthHeight_ = height;

                // texture is RGBA
                const int byteLength = depthWidth_ * depthHeight_ * sizeof(uint16_t);

                depthData_ = DepthPtr(new int16_t[byteLength]);
            }

            depthFrame.copy_to(&depthData_[0]);
        }
    }

    void update_mouse_position(sf::RenderWindow& window,
        const astra::CoordinateMapper& coordinateMapper)
    {
        const sf::Vector2i position = sf::Mouse::getPosition(window);
        const sf::Vector2u windowSize = window.getSize();

        float mouseNormX = position.x / float(windowSize.x);
        float mouseNormY = position.y / float(windowSize.y);

        mouseX_ = depthWidth_ * mouseNormX; //depth
        mouseY_ = depthHeight_ * mouseNormY; //depth mark

        if (mouseX_ >= depthWidth_ ||
            mouseY_ >= depthHeight_ ||
            mouseX_ < 0 ||
            mouseY_ < 0) {
            return;
        }

        const size_t index = (depthWidth_ * mouseY_ + mouseX_);
        const short z = depthData_[index];
        
        coordinateMapper.convert_depth_to_world(float(mouseX_),
            float(mouseY_),
            float(z),
            mouseWorldX_,
            mouseWorldY_,
            mouseWorldZ_);

        zvalue = mouseWorldZ_;
        xvalue = mouseWorldX_;
        yvalue = mouseWorldY_;
    }

    void draw_text(sf::RenderWindow& window,
        sf::Text& text,
        sf::Color color,
        const int x,
        const int y) const
    {
        text.setColor(sf::Color::Black);
        text.setPosition(x + 5, y + 5);
        window.draw(text);

        text.setColor(color);
        text.setPosition(x, y);
        window.draw(text);
    }

    void draw_mouse_overlay(sf::RenderWindow& window,
        const float depthWScale,
        const float depthHScale) const
    {
        if (!isMouseOverlayEnabled_ || !depthData_) { return; }

        std::stringstream str;
        str << std::fixed
            << std::setprecision(0)
            << "(" << mouseX_ << ", " << mouseY_ << ") "
            << "X: " << mouseWorldX_ << " Y: " << mouseWorldY_ << " Z: " << mouseWorldZ_;

        const int characterSize = 40;
        sf::Text text(str.str(), font_);
        text.setCharacterSize(characterSize);
        text.setStyle(sf::Text::Bold);

        const float displayX = 10.f;
        const float margin = 10.f;
        const float displayY = window.getView().getSize().y - (margin + characterSize);

        draw_text(window, text, sf::Color::White, displayX, displayY);
    }

    void draw_to(sf::RenderWindow& window, const astra::CoordinateMapper& coordinateMapper)
    {
        if (displayBuffer_ != nullptr)
        {
            const float depthWScale = window.getView().getSize().x / displayWidth_;
            const float depthHScale = window.getView().getSize().y / displayHeight_;

            sprite_.setScale(depthWScale, depthHScale);
            window.draw(sprite_);

            sf::RectangleShape rectangle;
            rectangle.setSize(sf::Vector2f((maxX - minX) * 2, (maxY - minY) * 2));
            rectangle.setOutlineColor(sf::Color::Red);
            rectangle.setFillColor(sf::Color(0, 0, 0, 0));
            rectangle.setOutlineThickness(5);
            rectangle.setPosition(minX * 2, minY * 2);
            window.draw(rectangle);

            //draw circle
            sf::CircleShape circle;
            circle.setRadius(10);
            circle.setOutlineColor(sf::Color::Blue);
            circle.setOutlineThickness(5);
            circle.setFillColor(sf::Color(0, 0, 0, 0));

            //to get center of front wheel, do min - 4
            //to get center of back wheel(minx), do max + 5
            //minX, minY, maxX, maxY
            //front one

            if ((maxX - minX) * 2 > 200)// in mm
            {
                //right wheel
                circle.setPosition((rectangle.getPosition().x) + 2 * (maxX - minX) - 20, (rectangle.getPosition().y) + 2 * (maxY - minY) - 10);
                window.draw(circle);
                //left wheel
                circle.setPosition((rectangle.getPosition().x) + 58, (rectangle.getPosition().y) + 2 * (maxY - minY) - 10);//same y values
                window.draw(circle);

                if ((circle.getPosition().x) <= 479 && shotsfired == false) {
                    screen_shotwindow(window, "RightWheelReachedPost_specialcar.png");
                    shotsfired = true;
                }
            }
            else
            {
                //right wheel
                circle.setPosition((rectangle.getPosition().x) + 2 * (maxX - minX) - 30, (rectangle.getPosition().y) + 2 * (maxY - minY) - 10);
                window.draw(circle);
                //left wheel
                circle.setPosition((rectangle.getPosition().x) + 10, (rectangle.getPosition().y) + 2 * (maxY - minY) - 10);//same y values
                window.draw(circle);

                if ((circle.getPosition().x) <= 497 && shotsfired == false) {
                    screen_shotwindow(window, "RightWheelReachedPost_normalcar.png");
                    shotsfired = true;
                }
            }

            //draw post
            rectangle.setPosition(229 * 2 + 21, 100 * 2);
            rectangle.setSize(sf::Vector2f(8 * 2, 200 * 2));
            rectangle.setOutlineThickness(2);
            window.draw(rectangle);

            draw_mouse_overlay(window, depthWScale, depthHScale);

            if (flag < n - 1)
            {
                double zeroed = minX - 5;
                double distance_to_start = (115 - (0.214 * zeroed - 7.33)) * 10;
                std::cout << std::setprecision(4) << "(Distance from start of conveyer " << distance_to_start << std::endl;
            }

            maxX = 0;
            maxY = 0;
            minX = 100000;
            minY = 100000;
        }
    }

    void toggle_paused()
    {
        isPaused_ = !isPaused_;
    }

    bool is_paused() const
    {
        return isPaused_;
    }

    void toggle_overlay()
    {
        isMouseOverlayEnabled_ = !isMouseOverlayEnabled_;
    }

    bool overlay_enabled() const
    {
        return isMouseOverlayEnabled_;
    }
};

astra::DepthStream configure_depth(astra::StreamReader& reader)
{
    auto depthStream = reader.stream<astra::DepthStream>();

    auto oldMode = depthStream.mode();

    //We don't have to set the mode to start the stream, but if you want to here is how:
    astra::ImageStreamMode depthMode;

    depthMode.set_width(640);
    depthMode.set_height(480);
    depthMode.set_pixel_format(astra_pixel_formats::ASTRA_PIXEL_FORMAT_DEPTH_MM);
    depthMode.set_fps(30);

    depthStream.set_mode(depthMode);

    auto newMode = depthStream.mode();
    printf("Changed depth mode: %dx%d @ %d -> %dx%d @ %d\n",
        oldMode.width(), oldMode.height(), oldMode.fps(),
        newMode.width(), newMode.height(), newMode.fps());

    return depthStream;
}

int main(int argc, char** argv)
{

    astra::initialize();

    set_key_handler();

    DepthFrameListener listener;

    std::cout << listener.zvalue << listener.xvalue << listener.yvalue; //outputs Z, X and Y


    sf::RenderWindow window(sf::VideoMode(1280, 960), "Depth Viewer");

#ifdef _WIN32
    auto fullscreenStyle = sf::Style::None;
#else
    auto fullscreenStyle = sf::Style::Fullscreen;
#endif

    const sf::VideoMode fullScreenMode = sf::VideoMode::getFullscreenModes()[0];
    const sf::VideoMode windowedMode(1280, 1024);
    bool isFullScreen = false;

    astra::StreamSet streamSet;
    astra::StreamReader reader = streamSet.create_reader();
    reader.stream<astra::PointStream>().start();

    auto depthStream = configure_depth(reader);
    depthStream.start();

    reader.add_listener(listener);

    auto coordinateMapper = depthStream.coordinateMapper();

    while (window.isOpen())
    {
        astra_update();

        using DepthPtr = std::unique_ptr<int16_t[]>;
        DepthPtr depthData_{ nullptr };

        sf::Event event;
        while (window.pollEvent(event))
        {
            switch (event.type)
            {
            case sf::Event::Closed:
                window.close();
                break;
            case sf::Event::KeyPressed:
            {
                if (event.key.code == sf::Keyboard::C && event.key.control)
                {
                    window.close();
                }
                switch (event.key.code)
                {
                case sf::Keyboard::D:
                {
                    auto oldMode = depthStream.mode();
                    astra::ImageStreamMode depthMode;

                    depthMode.set_width(640);
                    depthMode.set_height(400);
                    depthMode.set_pixel_format(astra_pixel_formats::ASTRA_PIXEL_FORMAT_DEPTH_MM);
                    depthMode.set_fps(30);

                    depthStream.set_mode(depthMode);
                    auto newMode = depthStream.mode();
                    printf("Changed depth mode: %dx%d @ %d -> %dx%d @ %d\n",
                        oldMode.width(), oldMode.height(), oldMode.fps(),
                        newMode.width(), newMode.height(), newMode.fps());
                    break;
                }
                case sf::Keyboard::Escape:
                    window.close();
                    break;
                case sf::Keyboard::F:
                    if (isFullScreen)
                    {
                        window.create(windowedMode, "Depth Viewer", sf::Style::Default);
                    }
                    else
                    {
                        window.create(fullScreenMode, "Depth Viewer", fullscreenStyle);
                    }
                    isFullScreen = !isFullScreen;
                    break;
                case sf::Keyboard::R:
                    depthStream.enable_registration(!depthStream.registration_enabled());
                    break;
                case sf::Keyboard::M:
                    // depthStream.enable_mirroring(!depthStream.mirroring_enabled());
                    break;
                case sf::Keyboard::P:
                    listener.toggle_paused();
                    break;
                case sf::Keyboard::Space:
                    listener.toggle_overlay();
                    break;
                default:
                    break;
                }
                break;
            }
            case sf::Event::MouseMoved:
            {
                //auto coordinateMapper = depthStream.coordinateMapper();
                listener.update_mouse_position(window, coordinateMapper);
                break;
            }
            default:
                break;
            }
        }

        // clear the window with black color
        window.clear(sf::Color::Black);

        listener.draw_to(window, coordinateMapper);
        window.display();

        if (!shouldContinue)
        {
            window.close();
        }
    }

    astra::terminate();

    return 0;
}
