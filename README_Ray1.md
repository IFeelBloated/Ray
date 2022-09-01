**README**
This solution does **NOT** use ***any*** OOP design. The code is structured in a highly functional way (although purity is not enforced) and makes heavy use of advanced metaprogramming techniques in C++. This results in extremely concise, highly flexible, and depending on personal taste, elegant code. In the real-life situation, some students might overpower your C++ knowledge and submit solutions like this. You should therefore be prepared to grade such solutions. **If you find the solution extremely hard to understand, or if you cannot grade the solution, please send your questions to Nick asap!!**

**Code Structure**
The entire solution is self-contained in a single file: Ray.hxx, none of the stencil interface (namely `RayTraceScene`, `RayTracer`, and `camera`) is used. There’re 3 major sections of the code, `ViewPlane`, `Ray`, and `ImplicitFunctions`, each has been grouped into their own namespace.

**ViewPlane**
`ViewPlane::ConfigureProjectorFromScreenSpaceIntoWorldSpace` is a higher order (and higher ranked) function. It has the signature (in Haskell syntax): `ConfigureProjectorFromScreenSpaceIntoWorldSpace :: CameraData -> Int -> Int -> (Num a => a -> a -> Vec4)`, it takes a camera object, canvas height and width as arguments, and returns a function that projects any `(x, y)` coordinates on the view plane into the world space.

**Ray**
`Ray::Intersect` is a single step ray tracing function. It has the signature `Intersect :: (Vec4 -> Vec4 -> (Double, Vec3)) -> Mat4 -> Vec4 -> Vec4 -> (Double, Vec4)`, the first two arguments are an implicit function and a cumulative transformation matrix, representing a shape primitive. The latter two arguments are the eye point and the ray direction, representing a ray. It returns `(t, WorldSpaceNormal)` if the ray intersects the shape, otherwise it returns `(∞, Undefined)`.

**ImplicitFunctions**
These are functions with the signature `Vec4 -> Vec4 -> (Double, Vec3)`. Implicit functions take an object space ray, and return `(t, ObjectSpaceNormal)` if there's an intersection, otherwise, they return `(∞, Undefined)`. Implicit functions are the combination of unbounded parametric surfaces (`ImplicitFunctions::Solvers`) and constraints (`ImplicitFunctions::StandardConstraints`).

**Notes**
Code might not compile on macOS if GCC (11 or later) is not installed. Clang doesn't yet have complete support for C++20, it is however guaranteed to compile under our standard grading environment, which has GCC11.