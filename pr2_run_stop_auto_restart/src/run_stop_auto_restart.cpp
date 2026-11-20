/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// When the run stop is re-enabled (released), switch the three breakers on and reset the motors,
// as ROS 1. The sequence runs in a worker thread: it waits for services and sleeps, which must not
// happen in a callback of the executor that has to deliver the service responses.

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <pr2_msgs/msg/power_board_state.hpp>
#include <pr2_power_board/srv/power_board_command.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_srvs/srv/empty.hpp>

using namespace std::chrono_literals;
using PowerBoardCommand = pr2_power_board::srv::PowerBoardCommand;
using Empty = std_srvs::srv::Empty;

class RunStopAutoRestart : public rclcpp::Node
{
public:
  RunStopAutoRestart()
  : rclcpp::Node("run_stop_helper")
  {
    power_control_ = create_client<PowerBoardCommand>("power_board/control");
    reset_motors_ = create_client<Empty>("pr2_ethercat/reset_motors");
    power_state_sub_ = create_subscription<pr2_msgs::msg::PowerBoardState>(
      "power_board/state", 1,
      [this](const pr2_msgs::msg::PowerBoardState & msg) { powerStateCallback(msg); });
  }

  ~RunStopAutoRestart() override
  {
    if (worker_.joinable()) worker_.join();
  }

private:
  void powerStateCallback(const pr2_msgs::msg::PowerBoardState & msg)
  {
    if (!last_e_stop_state_ && msg.run_stop) {
      if (busy_.exchange(true)) {
        RCLCPP_WARN(get_logger(), "Run stop re-enabled while still restarting; ignored.");
      } else {
        RCLCPP_INFO(get_logger(), "Run stop re-enabled. Bringing up power and resetting motors.");
        if (worker_.joinable()) worker_.join();
        worker_ = std::thread([this, serial = msg.serial_num]() {
          restart(serial);
          busy_ = false;
        });
      }
    }
    last_e_stop_state_ = msg.run_stop;
  }

  template<typename ClientT, typename RequestT>
  bool call(const ClientT & client, const RequestT & request)
  {
    auto future = client->async_send_request(request);
    if (future.wait_for(5s) != std::future_status::ready) {
      client->remove_pending_request(future);
      return false;
    }
    return true;
  }

  void restart(uint32_t serial)
  {
    // enable power
    if (!power_control_->wait_for_service(5s)) {
      RCLCPP_ERROR(get_logger(), "Could not find power board control service");
      return;
    }
    for (unsigned int i = 0; i < 3 && rclcpp::ok(); i++) {
      auto request = std::make_shared<PowerBoardCommand::Request>();
      request->serial_number = serial;
      request->command = "start";
      request->breaker_number = i;
      auto future = power_control_->async_send_request(request);
      if (future.wait_for(5s) != std::future_status::ready) {
        power_control_->remove_pending_request(future);
        RCLCPP_ERROR(get_logger(), "  - Enabling breaker %u: no answer", i);
      } else {
        RCLCPP_INFO(get_logger(), "  - Enabling breaker %u: %i", i, future.get()->retval);
      }
    }

    // reset motors
    std::this_thread::sleep_for(2s);  // give motors time to detect power up
    if (!rclcpp::ok()) return;
    if (!reset_motors_->wait_for_service(5s)) {
      RCLCPP_ERROR(get_logger(), "Could not find reset motors service");
      return;
    }
    if (call(reset_motors_, std::make_shared<Empty::Request>())) {
      RCLCPP_INFO(get_logger(), "  - Reset motors");
    } else {
      RCLCPP_ERROR(get_logger(), "  - Reset motors: no answer");
    }
  }

  bool last_e_stop_state_ = true;
  std::atomic<bool> busy_{false};
  std::thread worker_;
  rclcpp::Client<PowerBoardCommand>::SharedPtr power_control_;
  rclcpp::Client<Empty>::SharedPtr reset_motors_;
  rclcpp::Subscription<pr2_msgs::msg::PowerBoardState>::SharedPtr power_state_sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RunStopAutoRestart>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
