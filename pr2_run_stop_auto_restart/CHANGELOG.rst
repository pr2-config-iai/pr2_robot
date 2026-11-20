^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package pr2_run_stop_auto_restart
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

2.0.0 (unreleased)
------------------
* Port to ROS 2 (Jazzy), ament_cmake. Same behaviour and names (``power_board/state``,
  ``power_board/control``, ``pr2_ethercat/reset_motors``). The restart sequence (waits for the
  services, switches the breakers, sleeps 2 s, resets the motors) runs in a worker thread instead
  of the subscription callback, so the executor can deliver the service responses.

1.6.32 (2021-05-26)
-------------------

1.6.31 (2020-04-14)
-------------------
* fixed CMake errors
* Contributors: David Feil-Seifer

1.6.30 (2018-04-23)
-------------------

1.6.29 (2018-04-22)
-------------------

1.6.28 (2018-04-21)
-------------------

1.6.27 (2018-04-20)
-------------------

1.6.26 (2018-03-19)
-------------------

1.6.25 (2018-03-19)
-------------------
* updated packages for new maintainer
* updated changelogs
* Contributors: David Feil-Seifer

1.6.7 (2015-02-11)
------------------
* Compile pr2_run_stop_auto_restart after generating pr2_msgs c++ codes
* Contributors: Ryohei Ueda
