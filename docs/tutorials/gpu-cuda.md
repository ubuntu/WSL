# Enabling GPU acceleration with the NVIDIA CUDA Platform
*Authored by Carlos Nihelton ([carlos.santanadeoliveira@canonical.com](mailto:carlos.santanadeoliveira@canonical.com))*

While WSL's default setup allows you to develop cross-platform applications without leaving Windows, enabling GPU acceleration inside WSL provides users with direct access to the hardware. This provides support for GPU-accelerated AI/ML training and the ability to develop and test applications built on top of technologies, such as OpenVINO, OpenGL, and CUDA that target Ubuntu while staying on Windows.

## What you will learn:

* How to install a Windows graphical device driver compatible with WSL2
* How to install the NVIDIA CUDA toolkit for WSL 2 on Ubuntu
* How to compile and run a sample CUDA application on Ubuntu on WSL2

## What you will need:

* A Windows 10 version 21H2 or newer physical machine equipped with an NVIDIA graphics card and administrative permission to be able to install device drivers
* Ubuntu on WSL2 previously installed
* Familiarity with Linux command line utilities and interacting with Ubuntu on WSL2

> ⓘ Note: If you need more introductory topics, such as how to install Ubuntu on WSL, refer to previous tutorials that can be found [here for Windows 11](https://ubuntu.com/tutorials/install-ubuntu-on-wsl2-on-windows-11-with-gui-support#1-overview) and [here for Windows 10](https://ubuntu.com/tutorials/install-ubuntu-on-wsl2-on-windows-10#1-overview).


## Prerequisites

The following steps assume a specific hardware configuration. Although the concepts are essentially the same for other architectures, different hardware configurations will require the appropriate graphics drivers and CUDA toolkit.

Make sure the following prerequisites are met before moving forward:

* A physical machine with Windows 10 version 21H2 or higher
* NVIDIA’s graphic card
* Ubuntu 20.04 or higher installed on WSL 2
* Broadband internet connection able to download a few GB of data

## Install the appropriate Windows vGPU driver for WSL

> ⓘ Specific drivers are needed to enable use of a virtual GPU, which is how Ubuntu applications are able to access your GPU hardware, so you’ll need to follow this step even if your system drivers are up-to-date.

Please refer to the official [WSL documentation](https://docs.microsoft.com/en-us/windows/wsl/tutorials/gui-apps) for up-to-date links matching your specific GPU vendor. You can find these in `Install support for Linux GUI apps > Prerequisites` . For this example, we will download the `NVIDIA GPU Driver for WSL`.

![|624x283](https://lh3.googleusercontent.com/At632eOPirKgKd8OBD-sLfHui7WAa1lZSIDERr-BZNsqC28pAbX1dbAmbLbDO0aFQWvYShXJvwn42Pq7tvVkokWp5tl28oxoTlF-z0iyx3dLxiXYiq53wy17QgvxSD_Kh0Hd_l25)

> ⓘ **Note:** This is the only device driver you’ll need to install. Do not install any display driver on Ubuntu.

Once downloaded, double-click on the executable file and click `Yes` to allow the program to make changes to your computer.

![|624x136](https://lh4.googleusercontent.com/hsDq_ojZTfuOQrlUdHdJ3dpyjuzg2lSvN5idhz3QdXXvCjZo-cJwCf_fbwwy680q6ZsuryAm-D4c5nQhGh3NYRmFQp_7q0izm9Fszsb-kkCy852LICUqNXFFpbOasGFGicplhQ-T)

![|358x312](https://lh3.googleusercontent.com/ae6rEjwtpjWICy-udy6dVQSpxJdg8_Ql8LYojYiEPl_rJhQeEsppJTuViILfuHKRjiuH1q4pIuB-Wgwm-PQUiM68eQcAbiXrkc1WDbYZAXjsXHJFA9qD47E3guX2aEGr21yC8i9a)

Confirm the default directory and allow the self-extraction process to proceed.

![|369x175](https://lh5.googleusercontent.com/vEjsLLyFvRAPl4j62bXAmgbvicKJT_BsLoBtiHPSPYRgsDMd8RuFmpN1Sfbu_eOzxW2SOFBgiG4DPRQriTyfcmnFagu8z_IM47rvopk_o1f40paF2IdGWFU-7TdqowJe3tgQlGGh)

![|368x168](https://lh6.googleusercontent.com/PIGJXcLazCzqxl8vKoeWuFd0PxqQZ1KlsCRnPYEi8Yn3WmXlERJ6NUzFTSjijTuG8s_n02kbLtyfMGt2ThYJpEITjXiLQvfrrFV8sJc6YOii8QwDCsGX_rl0-m6UaTg_X4mYQ77I)

A splash screen appears with the driver version number and quickly turns into the main installer window. Read and accept the license terms to continue.

![|541x276](https://lh4.googleusercontent.com/SX3xhVRQNuWaVfpCmWI94VhZ8DuS8NIHw4ImHuWSmpr75tn-elq1pcuL4r-9CdpVKBbDCq3VgbC_QaaZi6PzJ_dTTPK2a3RrfTLcv7zrUHOngjZXLcIZwEVGoMlf83zUf_1-vZuz)

![|542x412](https://lh3.googleusercontent.com/fxMK_4rBBoIkmFbLGosmekl0BbnsILbGr3P8hERX4sQGH4x2yk93y7QyM5O-W-89Zkx00KealuDEk2cTJlosztG9u-Kb8CKQVfD7PbEETJMfCc4jZfIbvQLoR6lbOvD38p9zKuJ7)

Confirm the wizard defaults by clicking `Next` and wait until the end of the installation. You might be prompted to restart your computer.

![|536x397](https://lh3.googleusercontent.com/FQ5vOd2CNoPxksFJcuqHvpTWMP6lcu5a1vNZK6aK91EiDxMoCiCm4pq8Z5J5WZGptEObFg80L0m4KO7b3_DH8645DyMR-EvHbAM4wBzRmuBsADRthIE_udhF946AqUfSMoaHuTPb)

![|542x404](https://lh3.googleusercontent.com/FCfbRxBKnDdvVYKYTAtPA0ILSczxDmwpTmA4h3VxJWUVqgwIl9-Go1MZsYRn6trNar19oRFhWJFnVSS6Y9voOrBF9ht5na7lq8rmWKKOpK4_Q2ephb7-WnlVbLlEqOLAhPaQYyXY)

This step ends with a screen similar to the image below.

![|544x406](https://lh5.googleusercontent.com/RZDS3p48YaNDDnVkl7vgZRN_oY-qCgy5ptcYlcbc3_Gbo6ySrPumZVipUkCHvxJxgC73wKQy1uKw9co90_TQRkT_Cn8Q4Lz9ccWNJDzWWcCQNLPUmS1cMgvLpSVTA-fuilBIIdtb)

## Install NVIDIA CUDA on Ubuntu

> ⓘ Normally, CUDA toolkit for Linux will have the device driver for the GPU packaged with it. On WSL 2, the CUDA driver used is part of the Windows driver installed on the system, and, therefore, care must be taken `not` to install this Linux driver as previously mentioned.

The following commands will install the WSL-specific CUDA toolkit version 11.6 on Ubuntu 22.04 AMD64 architecture. Be aware that older versions of CUDA (<=10) don’t support WSL 2. Also notice that attempting to install the CUDA toolkit packages straight from the Ubuntu repository (`cuda`, `cuda-11-0`, or `cuda-drivers`) will attempt to install the Linux NVIDIA graphics driver, which is not what you want on WSL 2. So, first remove the old GPG key:

> `sudo apt-key del 7fa2af80`

Then setup the appropriate package for Ubuntu WSL:

> `wget https://developer.download.nvidia.com/compute/cuda/repos/wsl-ubuntu/x86_64/cuda-wsl-ubuntu.pin`

> `sudo mv cuda-wsl-ubuntu.pin /etc/apt/preferences.d/cuda-repository-pin-600`

> `sudo apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/wsl-ubuntu/x86_64/3bf863cc.pub`

> `sudo add-apt-repository 'deb https://developer.download.nvidia.com/compute/cuda/repos/wsl-ubuntu/x86_64/ /'`

> `sudo apt-get update`

> `sudo apt-get -y install cuda`

Once complete, you should see a series of outputs that end in `done.`:

![|544x559](https://lh3.googleusercontent.com/LOGRRLAHq7YA19ljM0eh0wpGwP1cXthB_bnDahTzxI3bziWb-qb9vZTvpAtEfKXUIghsgcNMvxTLz3xq2WquH_d_Fd34S6YAFM1UHCKjEuFTkL7nzMKAKYbDD-EInDpS2tjjZnQK7XIzijXDTg)

Congratulations! You should have a working installation of CUDA by now. Let’s test it in the next step.

## Compile a sample application

NVIDIA provides an open source repository on GitHub with samples for CUDA Developers to explore the features available in the CUDA Toolkit. Building one of these is a great way to test your CUDA installation. Let’s choose the simplest one just to validate that our installation works.

Let’s say you have a `~/Dev/` directory where you usually put your working projects. Navigate inside the directory and `git clone` the [cuda-samples repository](https://github.com/nvidia/cuda-samples):

> `cd ~/Dev`

> `git clone https://github.com/nvidia/cuda-samples`

To build the application, go to the cloned repository directory and type `make`:

> `cd ~/Dev/cuda-samples/Samples/1_Utilities/deviceQuery`

> `make`

A successful build will look like the screenshot below. Once complete, run the application:

![|624x135](https://lh5.googleusercontent.com/iN_jDkNiloSVVaGfK4Zr6nHOxa9vj-2aqKhf1jG0nxBmoN2YkA7sHXtaqiVGo8YB6hKlksq8oyzlLH1IitT6A6Jhq18D1PuwqRPRSF-bkaGTWIyTECtkO_XBzQcIMHIbeHJsX5QUHGWkloRu6w)

> `./deviceQuery`

You should see a similar output to the following detailing the functionality of your CUDA setup (the exact results depend on your hardware setup):

![|548x599](https://lh4.googleusercontent.com/0k7z_3i-WHJpebmYsRDCeHHh5DMdO-4xzsiPQz_jTuh4wRZV0-L7-5IiRlFLfIwku-VM2rKCdew_e2GieYloED-3jNEi-M8oByat6pasY7C3GHf7f3IegV2Q98faY-81w77m2Ix43BrZFBIAQw)

## Enjoy Ubuntu on WSL!

That’s all folks! In this tutorial, we’ve shown you how to enable GPU acceleration on Ubuntu on WSL 2 and demonstrated its functionality with the NVIDIA CUDA toolkit, from installation through to compiling and running a sample application.

We hope you enjoy using Ubuntu inside WSL for your Data Science projects. Don’t forget to check out [our blog](https://ubuntu.com/blog) for the latest news on all things Ubuntu.

### Further Reading

* [Setting up WSL for Data Science](https://ubuntu.com/blog/wsl-for-data-scientist)
* [Ubuntu WSL for Data Scientists Whitepaper](https://ubuntu.com/engage/ubuntu-wsl-for-data-scientists)
* [NVIDIA's CUDA Post Installation Actions](https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html#post-installation-actions)
* [Install Ubuntu on WSL2 on Windows 10 and Windows 11 with GUI Support](https://ubuntu.com/tutorials/install-ubuntu-on-wsl2-on-windows-11-with-gui-support#6-enjoy-ubuntu-on-wsl)
* [Microsoft WSL Documentation](https://docs.microsoft.com/en-us/windows/wsl/)
* [WSL on Ubuntu Wiki](https://wiki.ubuntu.com/WSL)
* [Ask Ubuntu](https://askubuntu.com/)