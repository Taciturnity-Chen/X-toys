/*
 * The Triangle class derived from Object3D.
 * Implemented by Mohism Research
 */

#include "Triangle.h"
#include "hit.h"

Triangle::Triangle(Vec3f &a, Vec3f &b, Vec3f &c, Material *m)
    : Object3D(m), mP0(a), mP1(b), mP2(c)
{

}

bool Triangle::intersect(const Ray &r, Hit &h, float tmin)
{
    float t;
    float a11 = mP0.x() - mP1.x();
    float a21 = mP0.y() - mP1.y();
    float a31 = mP0.z() - mP1.z();
    
    float a12 = mP0.x() - mP2.x();
    float a22 = mP0.y() - mP2.y();
    float a32 = mP0.z() - mP2.z();

    float a13 = r.getDirection().x();
    float a23 = r.getDirection().y();
    float a33 = r.getDirection().z();

    float b1 = mP0.x() - r.getOrigin().x();
    float b2 = mP0.y() - r.getOrigin().y();
    float b3 = mP0.z() - r.getOrigin().z();
    
    //Now we have all the coefficients of maxtrix equation
    /*
    *   | a11  a12  a13 |   | x1 |   | b1 |
    *   | a21  a22  a23 | * | x2 | = | b2 |
    *   | a31  a32  a33 |   | x3 |   | b3 |
    */
    
    float adj1 = a22*a33 - a23*a32;
    float adj2 = a13*a32 - a12*a33;
    float adj3 = a12*a23 - a13*a22;

    float DetA = a11*adj1 + a21*adj2 + a31*adj3;

    float beta = (b1*adj1 + b2*adj2 + b3*adj3)/DetA;

    if (beta <= 0.0f || beta >= 1.0f)
        return false;

    /*
    *       | a11  b1  a13 |
    * B2 =  | a21  b2  a23 |
    *       | a31  b3  a33 |
    */
    float bdj1 = a21*b3 - b2*a31;
    float bdj2 = a31*b1 - a11*b3;
    float bdj3 = a11*b2 - a21*b1;

    float gamma = (a13*bdj1 + a23*bdj2 + a33*bdj3)/DetA;

    if (gamma <= 0.0f || beta + gamma >= 1.0f)
        return false;

    /*
    *       | a11  a12  b1 |
    * B3 =  | a21  a22  b2 |
    *       | a31  a32  b3 |
    */
    t = -(a12*bdj1 + a22*bdj2 + a32*bdj3)/DetA;

    if (t  >= tmin)
    {
        Vec3f normal;
        Vec3f::Cross3(normal, (mP1 - mP0), (mP2 - mP0));
        normal.Normalize();
        h.set(t, mMaterial, normal, r);
        return true;
    }

    return false;   
}
