安卓 ES2 不支持 BackBuffer 作为 ShaderResource，会渲染失败，glBlitFramebuffer 也不支持。

所以改了一部分引擎代码来支持。

目前支持了 CPU 的 Stack Blur 和 GPU 的 Gaussian Blur。

 ![Gaussian Blur](https://raw.githubusercontent.com/nswutong/ScreenCapture/master/Intermediate/Test.png)