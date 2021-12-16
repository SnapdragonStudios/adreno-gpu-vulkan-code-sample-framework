## Framework

The framework folder contains the framework that is shared amongst the sample applications.

It handles basic app setup/teardown and provides a library of components that may be used by the samples. 
Ideally each sample should only have to implement the specific functionality it is demoing and no boiler-plate code.

To use the framework a sample app should statically link against framework and implement one function (that the framework calls into):
<pre><code>FrameworkApplicationBase* Application_ConstructApplication()
</pre></code>
The application should create its own base class derived from FrameworkApplicationBase to handle whatever functionality it needs.

The framework attempts to abstract away any platform specific code (such as input/event handling etc) through the FrameworkApplicationBase class.

Eg a simple Vulkan app would be:
<pre><code>class Application : public FrameworkApplicationBase
{
public:
    Application();
    ~Application() override;

private:
    std::unique_ptr<Vulkan> m_vulkan;
};

FrameworkApplicationBase* Application_ConstructApplication()
{
    return new Application();
}

Application::Application() : FrameworkApplicationBase()
{
    m_vulkan = std::make_unique<Vulkan>();
    assert( !m_vulkan->Init() );
}

Application::~Application()
{
}
</pre></code>
