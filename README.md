目前是把FBO写回CPU再模糊，性能很差，这是为了兼容 OpenGL ES2 前向渲染，ES2 上的 BackBuffer 作为 ShaderResource 会渲染失败，不知道原因，glBlitFramebuffer 也不支持。

目前支持了 CPU 的 Stack Blur 和 GPU 的 Gaussian Blur。

 ![Gaussian Blur](https://raw.githubusercontent.com/nswutong/ScreenCapture/master/Intermediate/Test.png)