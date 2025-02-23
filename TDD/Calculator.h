#ifdef LIB_EXPORTS
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif

class API Calculator {
public:
    int add(int a, int b);
    int subtract(int a, int b);
};