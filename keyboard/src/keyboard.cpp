#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include <keyboard_msgs/msg/key.hpp>
#include <SDL.h>

using namespace std::chrono_literals;

class KeyboardNode : public rclcpp::Node
{
public:
  KeyboardNode() : Node("keyboard_node")
  {
    initParameters();

    pub_down = this->create_publisher<keyboard_msgs::msg::Key>("keydown", 10);
    pub_up = this->create_publisher<keyboard_msgs::msg::Key>("keyup", 10);

    // Timer to check for keys pressed
    timer = this->create_wall_timer(20ms, std::bind(&KeyboardNode::timer_callback, this));

    if ( !allow_repeat_ ) repeat_delay_=0; // disable
    if (SDL_Init(SDL_INIT_VIDEO) < 0) throw std::runtime_error("Could not init SDL");
    SDL_EnableKeyRepeat( repeat_delay_, repeat_interval_ );
    SDL_WM_SetCaption("ROS keyboard input", NULL);
    window = SDL_SetVideoMode(100, 100, 0, 0);    

  }

private:
  void initParameters() 
  {
    this->declare_parameter("allow_repeat", false);
    allow_repeat_ = this->get_parameter("allow_repeat").as_bool();
    RCLCPP_INFO(this->get_logger(), "Allow repeat: %s", allow_repeat_ ? "true" : "false");

    this->declare_parameter("repeat_delay", SDL_DEFAULT_REPEAT_DELAY);
    repeat_delay_ = this->get_parameter("repeat_delay").as_int();
    RCLCPP_INFO(this->get_logger(), "Repeat delay: %d", repeat_delay_);

    this->declare_parameter("repeat_interval", SDL_DEFAULT_REPEAT_INTERVAL);
    repeat_interval_ = this->get_parameter("repeat_interval").as_int();
    RCLCPP_INFO(this->get_logger(), "Repeat interval: %d", repeat_interval_);
  }

  void timer_callback()
  {

    keyboard_msgs::msg::Key key;
    bool pressed;

    bool new_event = false;
  
    // Get event from SDL lib
    SDL_Event event;
    if (SDL_PollEvent(&event)) {
      switch(event.type) {
      case SDL_KEYUP:
        pressed_keys_[event.key.keysym.sym] = false;
        pressed = false;
        key.code = event.key.keysym.sym;
        key.modifiers = event.key.keysym.mod;
        new_event = true;
        SDL_FillRect(window, NULL,SDL_MapRGB(window->format, key.code, 0, 0));  
        SDL_Flip(window);   
	      break;
      case SDL_KEYDOWN:
        if (allow_repeat_ || !pressed_keys_[event.key.keysym.sym])
        {
          pressed_keys_[event.key.keysym.sym] = true;
          pressed = true;
          key.code = event.key.keysym.sym;
          key.modifiers = event.key.keysym.mod;
          new_event = true;
          SDL_FillRect(window, NULL, SDL_MapRGB(window->format,key.code,0,255)); 
          SDL_Flip(window);
        }
	      break;
      case SDL_QUIT:
        SDL_FreeSurface(window);
        SDL_Quit();
        timer->cancel();
        break;
      }
    }    

    if (new_event) {
      RCLCPP_DEBUG(this->get_logger(), "Key %d %s", key.code, pressed ? "pressed" : "released");
      key.header.stamp = this->get_clock()->now();
      if (pressed) pub_down->publish(key);
      else pub_up->publish(key);
    }	
  }

// Params and objects
private:
  rclcpp::TimerBase::SharedPtr timer;
  rclcpp::Publisher<keyboard_msgs::msg::Key>::SharedPtr pub_down;
  rclcpp::Publisher<keyboard_msgs::msg::Key>::SharedPtr pub_up;
  SDL_Surface* window;

  // Keep track of pressed keys as repeat_delay = 0 is deprecated in recent SDL versions
  std::unordered_map<SDLKey, bool> pressed_keys_;

  bool allow_repeat_ = false;
  int repeat_delay_ = SDL_DEFAULT_REPEAT_DELAY;
  int repeat_interval_ = SDL_DEFAULT_REPEAT_INTERVAL;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<KeyboardNode>());
  rclcpp::shutdown();
  return 0;
}
