#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable

#define MIP_LEVELS 4

float min3(float x, float y, float z)
{
    return min(min(x,y),z);
}

float max3( float x,  float y,  float z)
{
    return max(max(x,y),z);
}

#define IDENTIFICATION_BITS 10


struct obj_g_descriptor
{
    float4 world_pos;   ///w is blaenk
    float4 world_rot;   ///w is blaenk
    uint start;         ///internally used value
    uint tri_num;
    uint tid;           ///texture id
    uint size;
    uint mip_level_ids[MIP_LEVELS];
};



struct vertex
{
    float4 pos;
    float4 normal;
    float2 vt;
    float2 pad;
};

struct triangle
{
    struct vertex vertices[3];
};

float form(float x1, float y1, float x2, float y2, float x3, float y3, float x, float y, int which)
{

    if(which==1)
    {
        return fabs(((x2*y-x*y2)+(x3*y2-x2*y3)+(x*y3-x3*y))/2.0);
    }
    else if(which==2)
    {
        return fabs(((x*y1-x1*y)+(x3*y-x*y3)+(x1*y3-x3*y1))/2.0);
    }
    else if(which==3)
    {
        return fabs(((x2*y1-x1*y2)+(x*y2-x2*y)+(x1*y-x*y1))/2.0);
    }

    return fabs((x2*y1-x1*y2)+(x3*y2-x2*y3)+(x1*y3-x3*y1))/2.0;

} ///triangle equations

__constant float depth_far=140000; ///in non logarithmic coordinates
///1024 is 2^10
__constant uint mulint=UINT_MAX/1024;
__constant float depth_cutoff=0.22f;


float dcalc(float value)
{
    bool t=false;

    value=(log(value + 1)/(log(depth_far + 1)));

    return value;
}

float idcalc(float val)
{
    return (depth_far + 1)*exp(val) - 1;
}



int out_of_bounds(float val, float min, float max)
{
    if(val >= min && val < max)
    {
        return false;
    }
    return true;
}

float4 rot(float4 point, float4 c_pos, float4 c_rot)
{
    float4 ret;
    ret.x=      native_cos(c_rot.y)*(native_sin(c_rot.z)+native_cos(c_rot.z)*(point.x-c_pos.x)) - native_sin(c_rot.y)*(point.z-c_pos.z);
    ret.y=      native_sin(c_rot.x)*(native_cos(c_rot.y)*(point.z-c_pos.z)+native_sin(c_rot.y)*(native_sin(c_rot.z)*(point.y-c_pos.y)+native_cos(c_rot.z)*(point.x-c_pos.x)))+native_cos(c_rot.x)*(native_cos(c_rot.z)*(point.y-c_pos.y)-native_sin(c_rot.z)*(point.x-c_pos.x));
    ret.z=      native_cos(c_rot.x)*(native_cos(c_rot.y)*(point.z-c_pos.z)+native_sin(c_rot.y)*(native_sin(c_rot.z)*(point.y-c_pos.y)+native_cos(c_rot.z)*(point.x-c_pos.x)))-native_sin(c_rot.x)*(native_cos(c_rot.z)*(point.y-c_pos.y)-native_sin(c_rot.z)*(point.x-c_pos.x));

    return ret;

} ///pass in the value of sin(c_rot.y); etc





int very_roughly_equal_normal(float val, float val2)
{
    if(val > val2 - 0.05 && val < val2 + 0.05)
    {
        return true;
    }
    return false;
}

int vren(float a, float b)
{
    return very_roughly_equal_normal(a, b);
}

float rerror(float a, float b)
{
    return fabs(a-b);
}



uint return_max_num(int size)
{
    int max_tex_size=2048;

    return (max_tex_size/size) * (max_tex_size/size);

}


float4 read_tex_array(float4 coords, int tid, global uint *num, global uint *size, __read_only image3d_t array)
{

    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
               CLK_ADDRESS_CLAMP_TO_EDGE        |
               CLK_FILTER_NEAREST;

    int d=get_image_depth(array);

    float x=coords.x;
    float y=coords.y;

    //tid=tid-1; /////////////////////////////////////////////

    int slice = num[tid] >> 16;
    //slice=d-slice;
    int which = num[tid] & 0x0000FFFF;

    int max_tex_size=2048;




    //which=0;

    //slice=7;

    int width=size[slice];
    int bx=which % (max_tex_size/width);
    int by=which / (max_tex_size/width);

    //bx*=width;
    //by*=width;

    x=1-x;
    y=1-y;


    x*=width;
    y*=width;

    if(x<0)
        x=0;

    if(x>=width)
        x=width - 1;

    if(y<0)
        y=0;

    if(y>=width)
        y=width - 1;

    //slice=0;
    //width=2048;

    //width=1024;



    //int max_tex_size=2048;

    int hnum=max_tex_size/width;
    ///max horizontal and vertical nums

    float tnumx=which % hnum;
    float tnumy=which / hnum;


    //int tx = which*width/max_tex_size;
    //int ty = which*width;

    float tx=tnumx*width;
    float ty=tnumy*width;

    //tx=0;
    //ty=0;
    //slice=0;
    //tx=0;
    //ty=0;

    //tx=0, ty=0;





    float4 coord={tx + x, ty + y, slice, 0};

    uint4 col;
    col=read_imageui(array, sam, coord);
    float4 t;
    t.x=col.x/255.0f;
    t.y=col.y/255.0f;
    t.z=col.z/255.0f;

    return t;
}



float4 return_col(float4 coord, int which, __read_only image3d_t i256, __read_only image3d_t i512, __read_only image3d_t i1024, __read_only image3d_t i2048) ///takes a normalised input
{

    float4 col;
    uint4 colui;


    sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
               CLK_ADDRESS_CLAMP_TO_EDGE        |
               CLK_FILTER_NEAREST;

    coord.x=1.0-coord.x;
    coord.y=1.0-coord.y;


    if(which==0)
    {
        float width=get_image_width(i256);
        coord.x*=width;
        coord.y*=width;
        colui=read_imageui(i256, sam, coord);
    }

    else if(which==1)
    {
        float width=get_image_width(i512);
        coord.x*=width;
        coord.y*=width;
        colui=read_imageui(i512, sam, coord);
    }

    else if(which==2)
    {
        float width=get_image_width(i1024);
        coord.x*=width;
        coord.y*=width;
        colui=read_imageui(i1024, sam, coord);
    }

    else if(which==3)
    {
        float width=get_image_width(i2048);
        coord.x*=width;
        coord.y*=width;
        colui=read_imageui(i2048, sam, coord);
    }

    col.x=colui.x/255.0f;
    col.y=colui.y/255.0f;
    col.z=colui.z/255.0f;

    return col;


}

float4 return_smooth_col(float4 coord, int which, __read_only image3d_t i256, __read_only image3d_t i512, __read_only image3d_t i1024, __read_only image3d_t i2048) ///takes a normalised input
{

    float4 col;
    float4 coords[4];
    float width=pow(2.0, 8+which);
    int2 pos={floor(coord.x*width), floor(coord.y*width)};

    coords[0].x=pos.x, coords[0].y=pos.y;
    coords[1].x=pos.x+1, coords[1].y=pos.y;
    coords[2].x=pos.x, coords[2].y=pos.y+1;
    coords[3].x=pos.x+1, coords[3].y=pos.y+1;

    for(int i=0; i<4; i++)
    {
        coords[i].x/=width;
        coords[i].y/=width;
        coords[i].z=coord.z;
        col+=return_col(coords[i], which, i256, i512, i1024, i2048);
    }

    col/=4.0;


    return col;


}

float4 return_bilinear_col_2(float4 coord, float width, int which, __read_only image3d_t i256, __read_only image3d_t i512, __read_only image3d_t i1024, __read_only image3d_t i2048) ///takes a normalised input
{

    /*u = u * tex.size - 0.5;
   v = v * tex.size - 0.5;
   int x = floor(u);
   int y = floor(v);
   double u_ratio = u - x;
   double v_ratio = v - y;
   double u_opposite = 1 - u_ratio;
   double v_opposite = 1 - v_ratio;
   double result = (tex[x][y]   * u_opposite  + tex[x+1][y]   * u_ratio) * v_opposite +
                   (tex[x][y+1] * u_opposite  + tex[x+1][y+1] * u_ratio) * v_ratio;*/



    float4 mcoord;
    mcoord.x=coord.x*width - 0.5;
    mcoord.y=coord.y*width - 0.5;
    mcoord.z=coord.z;

    float4 coords[4];

    int2 pos={floor(mcoord.x), floor(mcoord.y)};

    coords[0].x=pos.x, coords[0].y=pos.y;
    coords[1].x=pos.x+1, coords[1].y=pos.y;
    coords[2].x=pos.x, coords[2].y=pos.y+1;
    coords[3].x=pos.x+1, coords[3].y=pos.y+1;



    float4 colours[4];
    for(int i=0; i<4; i++)
    {
        coords[i].x/=width;
        coords[i].y/=width;
        coords[i].z=coord.z;

        colours[i]=return_col(coords[i], which, i256, i512, i1024, i2048);
    }


    //return_col(coord, which, i256, i512, i1024, i2048)

    float2 uvratio={mcoord.x-pos.x, mcoord.y-pos.y};

    float2 buvr={1.0-uvratio.x, 1.0-uvratio.y};

    float4 result;
    result.x=(colours[0].x*buvr.x + colours[1].x*uvratio.x)*buvr.y + (colours[2].x*buvr.x + colours[3].x*uvratio.x)*uvratio.y;
    result.y=(colours[0].y*buvr.x + colours[1].y*uvratio.x)*buvr.y + (colours[2].y*buvr.x + colours[3].y*uvratio.x)*uvratio.y;
    result.z=(colours[0].z*buvr.x + colours[1].z*uvratio.x)*buvr.y + (colours[2].z*buvr.x + colours[3].z*uvratio.x)*uvratio.y;


    return result;

}

float4 return_bilinear_col(float4 coord, uint tid, global uint *nums, global uint *sizes, __read_only image3d_t array) ///takes a normalised input
{


    float4 mcoord;

    int which=nums[tid];
    float width=sizes[which >> 16];

    mcoord.x=coord.x*width - 0.5;
    mcoord.y=coord.y*width - 0.5;
    mcoord.z=coord.z;

    float4 coords[4];

    int2 pos={floor(mcoord.x), floor(mcoord.y)};

    coords[0].x=pos.x, coords[0].y=pos.y;
    coords[1].x=pos.x+1, coords[1].y=pos.y;
    coords[2].x=pos.x, coords[2].y=pos.y+1;
    coords[3].x=pos.x+1, coords[3].y=pos.y+1;



    float4 colours[4];
    for(int i=0; i<4; i++)
    {
        coords[i].x/=width;
        coords[i].y/=width;
        coords[i].z=coord.z;

        //colours[i]=return_col(coords[i], which, i256, i512, i1024, i2048);
        colours[i]=read_tex_array(coords[i], tid, nums, sizes, array);
    }


    //return_col(coord, which, i256, i512, i1024, i2048)

    float2 uvratio={mcoord.x-pos.x, mcoord.y-pos.y};

    float2 buvr={1.0-uvratio.x, 1.0-uvratio.y};

    float4 result;
    result.x=(colours[0].x*buvr.x + colours[1].x*uvratio.x)*buvr.y + (colours[2].x*buvr.x + colours[3].x*uvratio.x)*uvratio.y;
    result.y=(colours[0].y*buvr.x + colours[1].y*uvratio.x)*buvr.y + (colours[2].y*buvr.x + colours[3].y*uvratio.x)*uvratio.y;
    result.z=(colours[0].z*buvr.x + colours[1].z*uvratio.x)*buvr.y + (colours[2].z*buvr.x + colours[3].z*uvratio.x)*uvratio.y;


    return result;

}

void texture_filter(global struct triangle* triangles, uint id, uint tid, int2 spos, float4 vt, float depth, float4 c_pos, float4 c_rot, float4 *col, int tid2, global uint* mipd , global uint *nums, global uint *sizes, __read_only image3d_t array)
{

    ///find z coord of texture pixel, work out distance of transition between the adjacent 2 texture levels (+ which 2 texture levels), bilinearly interpolate the two pixels, then interpolate the result

    global struct triangle *tri;
    tri=&triangles[id];

    int width=800;
    int height=600;

    float4 rotpoints[3];
    for(int j=0; j<3; j++)
    {
        rotpoints[j]=rot(tri->vertices[j].pos, c_pos, c_rot);

        float rx;
        rx=rotpoints[j].x * (700.0f/rotpoints[j].z);
        float ry;
        ry=rotpoints[j].y * (700.0f/rotpoints[j].z);

        rx+=width/2.0f;
        ry+=height/2.0f;


        rotpoints[j].x=rx;
        rotpoints[j].y=ry;
        //rotpoints[j].z=dcalc(rotpoints[j].z);
        //i want linear z coordinates
    }

    float adepth=(rotpoints[0].z + rotpoints[1].z + rotpoints[2].z)/3.0f;

    ///maths works out to be z = 700*n where n is 1/2, 1/4 of the size etc
    ///so, if z of pixel is between 0-700, use least, then first, then second, etc

    int tids[MIP_LEVELS+1];
    tids[0]=tid2;

    for(int i=1; i<MIP_LEVELS+1; i++)
    {
        tids[i]=mipd[i-1];
    }

    float mipdistance=700;

    float part=0;

    if(adepth<1)
    {
        adepth=1;
    }

    int wc=0;
    int wc1;
    float mdist;
    float fdist;

    int slice=nums[tid2] >> 16;
    int twidth=sizes[slice];



    float minvx=min3(rotpoints[0].x, rotpoints[1].x, rotpoints[2].x); ///these are screenspace coordinates, used relative to each other so +width/2.0 cancels
    float maxvx=max3(rotpoints[0].x, rotpoints[1].x, rotpoints[2].x);

    float minvy=min3(rotpoints[0].y, rotpoints[1].y, rotpoints[2].y);
    float maxvy=max3(rotpoints[0].y, rotpoints[1].y, rotpoints[2].y);

    float mintx=min3(tri->vertices[0].vt.x, tri->vertices[1].vt.x, tri->vertices[2].vt.x);
    float maxtx=max3(tri->vertices[0].vt.x, tri->vertices[1].vt.x, tri->vertices[2].vt.x);

    float minty=min3(tri->vertices[0].vt.y, tri->vertices[1].vt.y, tri->vertices[2].vt.y);
    float maxty=max3(tri->vertices[0].vt.y, tri->vertices[1].vt.y, tri->vertices[2].vt.y);

    float txdif=maxtx-mintx;
    float tydif=maxty-minty;


    float vxdif=maxvx-minvx;
    float vydif=maxvy-minvy;

    float xtexelsperpixel=txdif*twidth/vxdif;
    float ytexelsperpixel=tydif*twidth/vydif;

    float texelsperpixel = xtexelsperpixel > ytexelsperpixel ? xtexelsperpixel : ytexelsperpixel;


    float effectivedistance=mipdistance*texelsperpixel;

    if(effectivedistance<0)
    {
        return;
    }

    float corrected_depth=adepth + effectivedistance;




    for(int i=0; i<5; i++) //fundementally broken, using width of polygon when it needs to be using
                            //texture width to calculate miplevel
    {
        wc=i;
        if(i==4)
        {
            mdist=(pow(2.0f, (float)i)-1)*mipdistance;
            fdist=(pow(2.0f, (float)i)-1)*mipdistance;
            break;
        }

        mdist=(pow(2.0f, (float)i)-1)*mipdistance;
        fdist=(pow(2.0f, (float)i+1.0f)-1)*mipdistance;

        if(corrected_depth > mdist && corrected_depth < fdist)
        {
            break;
        }
    }

    wc1=wc+1;
    part=(fdist-corrected_depth)/(fdist-mdist);

    if(wc==4)
    {
        wc1=4;
        part=1;
    }


    ///how far away from the upper mip level are we, between 0 and 1;
    //x, y, tid, num, size, array;

    float4 coord={(float)vt.x, (float)vt.y, 0, 0};


    float4 col1=return_bilinear_col(coord, tids[wc], nums, sizes, array);

    //float4 col2=return_bilinear_col(coord, tids[wc1], nums, sizes, array);
    float4 col2=return_bilinear_col(coord, tids[wc1], nums, sizes, array);


    float4 final=col1*(part) + col2*(1-part);

    col->x=final.x;
    col->y=final.y;
    col->z=final.z;
}

void texture_filter_2(global struct triangle* triangles, uint id, uint tid, int2 spos, float depth, float4 c_pos, float4 c_rot, float4 *col, int tid2, global uint* mipd, global uint *nums, global uint *sizes, __read_only image3d_t array) ///bring triangle to screenspace

//void texture_filter(global struct triangle* triangles, uint id, uint tid, int2 spos, float4 vt, float depth, float4 c_pos, float4 c_rot, float4 *col, int tid2, global uint* mipd , global uint *nums, global uint *sizes, __read_only image3d_t array)
{

    int width=800;
    int height=600;


    int2 spss[4];
    for(int i=0; i<4; i++)
    {
        spss[i].x=(spos.x-0.5);
        spss[i].y=(spos.y-0.5);
    }

    spss[1].x+=1;
    spss[2].y+=1;
    spss[3].x+=1;
    spss[3].y+=1;



    global struct triangle *tri;
    tri=&triangles[id];


    float4 rotpoints[3];

    for(int j=0; j<3; j++)
    {

        rotpoints[j]=rot(tri->vertices[j].pos, c_pos, c_rot);

        float rx;
        rx=rotpoints[j].x * (700.0f/rotpoints[j].z);
        float ry;
        ry=rotpoints[j].y * (700.0f/rotpoints[j].z);

        rx+=width/2.0f;
        ry+=height/2.0f;


        rotpoints[j].x=rx;
        rotpoints[j].y=ry;
        rotpoints[j].z=dcalc(rotpoints[j].z);
    }


    int y1;
    y1 = round(rotpoints[0].y);
    int y2;
    y2 = round(rotpoints[1].y);
    int y3;
    y3 = round(rotpoints[2].y);

    int x1;
    x1 = round(rotpoints[0].x);
    int x2;
    x2 = round(rotpoints[1].x);
    int x3;
    x3 = round(rotpoints[2].x);

    int miny;
    miny=min3(y1, y2, y3)-1; ///oh, wow
    int maxy;
    maxy=max3(y1, y2, y3);
    int minx;
    minx=min3(x1, x2, x3)-1;
    int maxx;
    maxx=max3(x1, x2, x3);

    ///triangle is rotated into screenspace, use coordinates as interpolation values and rerr. It would be better to find the range of texture coordinates and step through that, though

    int rconstant=(x2*y3+x1*(y2-y3)-x3*y2+(x3-x2)*y1);
    //float area=form(x1, y1, x2, y2, x3, y3, 0, 0, 0);

    if(rconstant==0)
    {
        return;
    }

    int num=0;

    float4 tcoord[4];

    float4 colsum={0,0,0,0};

    for(int i=0; i<4; i++)
    {

        float x=spss[i].x;
        float y=spss[i].y;

        //float s1=fabs(((x2*y-x*y2)+(x3*y2-x2*y3)+(x*y3-x3*y))/2.0) + fabs(((x*y1-x1*y)+(x3*y-x*y3)+(x1*y3-x3*y1))/2.0) + fabs(((x2*y1-x1*y2)+(x*y2-x2*y)+(x1*y-x*y1))/2.0);

        //if(s1 < area + 0.001 && s1 > area - 0.001) /// DO NOT USE 0.0001 THIS CAUSES HOELELELELEL
        {
            num++;


            float f1=1.0f/rotpoints[0].z;
            float f2=1.0f/rotpoints[1].z;
            float f3=1.0f/rotpoints[2].z;

            float A=native_divide((f2*y3+f1*(y2-y3)-f3*y2+(f3-f2)*y1),rconstant);
            float B=native_divide(-(f2*x3+f1*(x2-x3)-f3*x2+(f3-f2)*x1),rconstant);
            float C=f1-A*x1 - B*y1;

            float ldepth=A*x + B*y + C;



            f1=tri->vertices[0].vt.x/rotpoints[0].z;
            f2=tri->vertices[1].vt.x/rotpoints[1].z;
            f3=tri->vertices[2].vt.x/rotpoints[2].z;

            A=native_divide((f2*y3+f1*(y2-y3)-f3*y2+(f3-f2)*y1),rconstant);
            B=native_divide(-(f2*x3+f1*(x2-x3)-f3*x2+(f3-f2)*x1),rconstant);
            C=f1-A*x1 - B*y1;

            float vtx=A*x + B*y + C;



            f1=tri->vertices[0].vt.y/rotpoints[0].z;
            f2=tri->vertices[1].vt.y/rotpoints[1].z;
            f3=tri->vertices[2].vt.y/rotpoints[2].z;

            A=native_divide((f2*y3+f1*(y2-y3)-f3*y2+(f3-f2)*y1),rconstant);
            B=native_divide(-(f2*x3+f1*(x2-x3)-f3*x2+(f3-f2)*x1),rconstant);
            C=f1-A*x1 - B*y1;

            float vty=A*x + B*y + C;



            float4 vt={(vtx/ldepth), (vty/ldepth), tid, 0};

            //colsum+=return_col(vt, which, i256, i512, i1024, i2048);

            tcoord[num-1]=vt;

        }

    }

    //float power=8+which; ///find size of image
    //float tw=pow(2.0f, power);

    int slice = nums[tid] >> 16;
    float tw    = sizes[slice];


    ///triangle is now rotated into screenspace



    for(int i=0; i<4; i++)
    {
        tcoord[i]*=tw;
    }


    ///which 0 is is 256, 512, 1024, 2048




    float n1=0;
    float mitx=min(min(tcoord[0].x, tcoord[1].x), min(tcoord[2].x, tcoord[3].x));
    float matx=max(max(tcoord[0].x, tcoord[1].x), max(tcoord[2].x, tcoord[3].x));

    float mity=min(min(tcoord[0].y, tcoord[1].y), min(tcoord[2].y, tcoord[3].y));
    float maty=max(max(tcoord[0].y, tcoord[1].y), max(tcoord[2].y, tcoord[3].y));



    if(mitx==matx || mity==maty)
    {
        return;
    }

    float step_size=6;

    /*float adepth=(rotpoints[0].z + rotpoints[1].z + rotpoints[2].z) / 3.0f;

    if(fabs(idcalc(adepth))<1000) ///does not work, figure out why
    {
        step_size=0;
    }*/

    float xstep=(matx - mitx) / step_size;
    float ystep=(maty - mity) / step_size;


    if(xstep<1.0)
        xstep=1.0;
    if(ystep<1.0)
        ystep=1.0;

    //xstep=round(xstep);
    //ystep=round(xstep);


    float xcentre=(mitx + matx)/2.0f;
    float ycentre=(mity + maty)/2.0f;

    float2 ttcoord;

    float div=0;

    //float maxdist2=sqrt(xcentre*xcentre + ycentre*ycentre);


    for(float i=mitx ; i<matx ; i+=(xstep)) //slow as poop
    {
        for(float j=mity ; j<maty ; j+=(ystep)) ///perhaps the only solution is bilinear filtering
                                                ///Nope, this needs fixing
        {
            //float xd2c=(i-xcentre)*(i-xcentre);
            //float yd2c=(j-ycentre)*(j-ycentre);
            //float itc=
            //if(fast_sqrt(xd2c+yd2c) < ycentre)
            //if(hypot(i-xcentre, j-ycentre)<ycentre)

            ttcoord.x=i-xcentre;
            ttcoord.y=j-ycentre;

            float len=fast_length(ttcoord);

            //if(len<ycentre || len<xcentre)
            {
                float4 vt={(float)(i)/tw, (float)(j)/tw, tid, 0};
                //colsum+=return_bilinear_col(vt, tw, which, i256, i512, i1024, i2048);

                //float distance2 = sqrt(ttcoord.x*ttcoord.x + ttcoord.y*ttcoord.y);

                //float weight=(distance2/maxdist2)*40;



                //colsum+=return_col(vt, which, i256, i512, i1024, i2048);
                colsum+=read_tex_array(vt, tid2, nums, sizes, array);
                //div+=weight;
                n1+=1;
            }
        }
    }

    //colsum/=div;


    colsum.x/=n1;
    colsum.y/=n1;
    colsum.z/=n1;

    *col=colsum;




}





//__kernel void part2(global struct triangle* triangles, global uint* depth_buffer, global uint* id_buffer, global float4* normal_map, global uint4* texture_map, global float4* c_pos, global float4* c_rot, __write_only image2d_t screen, __read_only image3d_t i256, __read_only image3d_t i512, __read_only image3d_t i1024, __read_only image3d_t i2048)
//__kernel void part2(global struct triangle* triangles, global float4 *c_pos, global float4 *c_rot, global uint* depth_buffer, global uint* id_buffer, global float4* normal_map, global uint4* texture_map, global struct obj_g_descriptor* gobj, global uint * gnum, __write_only image2d_t screen, __read_only image3d_t i256, __read_only image3d_t i512, __read_only image3d_t i1024, __read_only image3d_t i2048)
float interpolate(float f1, float f2, float f3, int x, int y, int x1, int x2, int x3, int y1, int y2, int y3, int rconstant)
{
    float A=native_divide((f2*y3+f1*(y2-y3)-f3*y2+(f3-f2)*y1),rconstant);
    float B=native_divide(-(f2*x3+f1*(x2-x3)-f3*x2+(f3-f2)*x1),rconstant);
    float C=f1-A*x1 - B*y1;

    float cdepth=(A*x + B*y + C);
    return cdepth;
}

void full_rotate(global struct triangle *triangles, int trid, global float4 *c_pos, global float4 *c_rot, float4 *out)
{
    int width=800;
    int height=600;

    __global struct triangle *cur_tri=&triangles[trid]; ///shared memory?

    out[0]=rot(cur_tri->vertices[0].pos, *c_pos, *c_rot);
    out[1]=rot(cur_tri->vertices[1].pos, *c_pos, *c_rot);
    out[2]=rot(cur_tri->vertices[2].pos, *c_pos, *c_rot);
    //-Xptxas -dlcm=cg

    for(int j=0; j<3; j++)
    {
        float rx;
        rx=out[j].x * (700.0f/out[j].z);
        float ry;
        ry=out[j].y * (700.0f/out[j].z);

        rx+=width/2.0f;
        ry+=height/2.0f;


        out[j].x=rx;
        out[j].y=ry;
        out[j].z=dcalc(out[j].z);
    }

}


uint find_id(global struct triangle* triangles, global float4 *c_pos, global float4 *c_rot, uint4 ccoord, global uint *tri_num)
{
   uint id = ccoord.z & (0x3FF);
   uint depth = (ccoord.z & (0x3FF ^ 0xFFFFFFFF)) >> IDENTIFICATION_BITS;
   float fdepth=(float)depth/mulint;
   float pdepth=idcalc(fdepth);

   float4 fcoord={ccoord.x, ccoord.y, pdepth, 0};

   float4 wcoord;

   wcoord.x=fcoord.x*fcoord.z/700.0f;
   wcoord.y=fcoord.y*fcoord.z/700.0f;
   wcoord.z=fcoord.z;
   wcoord.w=0;

   float4 negative_camera=-(*c_rot);

   float4 woc=rot(wcoord, *c_pos, negative_camera);


   uint possible_ids=(*tri_num)/pow(2.0, (float)IDENTIFICATION_BITS);

   for(uint i=id; i<id+possible_ids && i < *tri_num; i++)
   {
        global struct triangle *T=&triangles[i];
        uint lx=min3(T->vertices[0].pos.x, T->vertices[1].pos.x, T->vertices[2].pos.x);
        uint ly=min3(T->vertices[0].pos.y, T->vertices[1].pos.y, T->vertices[2].pos.y);
        uint lz=min3(T->vertices[0].pos.z, T->vertices[1].pos.z, T->vertices[2].pos.z);

        uint mx=max3(T->vertices[0].pos.x, T->vertices[1].pos.x, T->vertices[2].pos.x);
        uint my=max3(T->vertices[0].pos.y, T->vertices[1].pos.y, T->vertices[2].pos.y);
        uint mz=max3(T->vertices[0].pos.z, T->vertices[1].pos.z, T->vertices[2].pos.z);

        if(woc.x > lx && woc.x < mx && woc.y > ly && woc.y < my && woc.z > lz && woc.z < mz)
        {
            return i;
        }

   }

   return 0;


}


__kernel void part2(global struct triangle* triangles, global uint *tri_num, global float4 *c_pos, global float4 *c_rot, global uint* depth_buffer, global uint* id_buffer, global float4* normal_map, global uint4* texture_map, global struct obj_g_descriptor* gobj, global uint * gnum, __write_only image2d_t screen, global int *nums, global int *sizes, __read_only image3d_t array)
{
    //perform deferred part of renderer

    unsigned int tx=get_global_id(0);
    unsigned int ty=get_global_id(1);



    unsigned int width=800;
    unsigned int height=600;

    if(tx < width-1 && ty < height-1 && tx>0 && ty>0) ///of course, if you try to write outside the screen then you are a fucking moron, moron >.> FIXED
    {
       __global float4 *fn=&normal_map[ty*width + tx];

       //volatile




        int2 coord={tx, ty};
        float4 col={1.0f, 1.0f, 1.0f, 1.0f};



        float4 halfnormals;

        float4 normals=normal_map[ty*width + tx];

        float hnc=0;

        for(int i=-1; i<2; i++)
        {
            for(int j=-1; j<2; j++)
            {
                if(abs(j)!=abs(i))// || (i==0 && j==0))
                {
                    continue;
                }
                hnc+=1;
                halfnormals+=normal_map[(ty+j)*width + tx + i];
            }

        }
        halfnormals/=hnc;
        halfnormals=normalize(halfnormals);


        //float nerror=sqrt(pow(rerror(halfnormals.x, normals.x), 2) + pow(rerror(halfnormals.y, normals.y), 2) + pow(rerror(halfnormals.z, normals.z), 2));
        float nerror=rerror(halfnormals.x, normals.x) + rerror(halfnormals.y, normals.y) + rerror(halfnormals.z, normals.z);

        float4 anormal=*fn;


        if(nerror>1.0)
        {
            // *fn=halfnormals;
            anormal=halfnormals;
        }



        //int ftmz=texture_map[ty*width + tx].z;

        int ftsum=0;




        /*for(int i=-1; i<2; i++)
        {
            //for(int j=-1; j<2; j++)
            {
                //if(abs(j)==abs(i))
                {
                   // continue;
                }
                int cz=texture_map[(ty)*width + tx + i].z;
                ftsum+=cz;
            }
        }

        ftsum=round(ftsum/3.0f);
        ftmz=ftsum;*/



        if(depth_buffer[ty*width + tx]!=UINT_MAX) ///DO_SHIT
        {

            __global uint *ft=&depth_buffer [ty*width + tx];
            //__global uint *fi=&id_buffer    [ty*width + tx];
            __global uint4 *ftm=&texture_map[ty*width + tx];


            int ftmz=(*ftm).z;

            int idflag=0;

            uint possible_id=((*ft) & (0x3FF));
            uint buf_id=(*ftm).w;

            uint bufidm=(buf_id >> (32-IDENTIFICATION_BITS)) & 0x3FF; ///Id buffer doesn't look like the thing to correct, o_id instead?

            if(bufidm != possible_id)
            {
                //depth_buffer[ty*width + tx]=UINT_MAX;
               //return;
               idflag=1;
            }
            else
            {
                depth_buffer[ty*width + tx]=UINT_MAX;
                //return;
            }

            /*if((buf_id >> IDENTIFICATION_BITS) & 0x3FF == possible_id)
            {
                depth_buffer[ty*width + tx]=UINT_MAX;
                return;
            }*/




            float4 rp[3];

            global struct triangle *ctri=&triangles[buf_id];

            full_rotate(triangles, buf_id, c_pos, c_rot, rp);


            int y1 = round(rp[0].y);
            int y2 = round(rp[1].y);
            int y3 = round(rp[2].y);

            int x1 = round(rp[0].x);
            int x2 = round(rp[1].x);
            int x3 = round(rp[2].x);

            int miny=min3(y1, y2, y3)-1;
            int maxy=max3(y1, y2, y3);
            int minx=min3(x1, x2, x3)-1;
            int maxx=max3(x1, x2, x3);
            int rconstant=(x2*y3+x1*(y2-y3)-x3*y2+(x3-x2)*y1);

            anormal.x=interpolate(ctri->vertices[0].normal.x, ctri->vertices[1].normal.x, ctri->vertices[2].normal.x, tx, ty, x1, x2, x3, y1, y2, y3, rconstant);
            anormal.y=interpolate(ctri->vertices[0].normal.y, ctri->vertices[1].normal.y, ctri->vertices[2].normal.y, tx, ty, x1, x2, x3, y1, y2, y3, rconstant);
            anormal.z=interpolate(ctri->vertices[0].normal.z, ctri->vertices[1].normal.z, ctri->vertices[2].normal.z, tx, ty, x1, x2, x3, y1, y2, y3, rconstant);







            uint ttid=0;

            //uint thisid=*fi;

            uint thisid=(*ftm).w;
            //uint4 ccoord={tx, ty, *ft, 0};
            //thisid=find_id(triangles, c_pos, c_rot, ccoord, tri_num);

            //if(thisid==0)
            //{
            //    return;
            //}




            ttid=gobj[ftmz].tid;

            float4 l1={0,300,0,0};
            //float4 l1={c_pos->x,c_pos->y,c_pos->z,0};
            l1=rot(l1, *c_pos, *c_rot);

            /*l1=rot(l1, *c_pos, *c_rot);
            l1.x=l1.x*700.0/l1.z;
            l1.y=l1.*700.0/l1.z;
            l1.x+=width/2.0;
            l1.y+=height/2.0;*/

            float4 point = {tx, ty, idcalc((float)(*ft)/(float)UINT_MAX), 0};

            float4 normal;

            sampler_t sam = CLK_NORMALIZED_COORDS_FALSE |
                           CLK_ADDRESS_CLAMP_TO_EDGE    |
                           CLK_FILTER_NEAREST;



            float4 l2p=l1-point;

            float light;
            light=dot(fast_normalize(l2p), fast_normalize(anormal));
            light/=2.0f;
            light+=0.5;


            //uint4 t_map=*ftm;

            float4 vt={(float)(*ftm).x/mulint, (float)(*ftm).y/mulint, 0, 0};



            float4 tcol;

            uint depth=*ft;
            depth = depth >> IDENTIFICATION_BITS;




            texture_filter(triangles, thisid, ttid, coord, vt, depth, *c_pos, *c_rot, &tcol, gobj[ftmz].tid, gobj[ftmz].mip_level_ids, nums, sizes, array);
            //if(idflag==1)
            {
                tcol.x=255;
                tcol.y=255;
                tcol.z=255;
            }

            write_imagef(screen, coord, tcol*light);

        }                                        ///END_DO_SHIT

        depth_buffer[ty*width + tx]=UINT_MAX;

    }
    else if((tx==0 || ty==0 || tx==width-1 || ty==height-1) && tx < width && ty < height)
    {
        int2 coord={tx, ty};
        float4 col={0,0,0,0};
        write_imagef(screen, coord, col);
        depth_buffer[ty*width + tx]=UINT_MAX;
    }





}


__kernel void part1(global struct triangle* triangles, global float4* pc_pos, global float4* pc_rot, global uint* tri_num, global uint* depth_buffer, global float4* normal_map, global uint* id_buffer, global uint4* texture_map, global struct obj_g_descriptor* desc, global uint* num)//, __write_only image2d_t screen)
{
    //rotate triangles into screenspace and draw them

    unsigned int width=800;
    unsigned int height=600;

    int2 coordtest={0,0};
    float4 tcol={1.0f, 1.0f, 1.0f, 1.0f};

    unsigned int o_id=0;
    unsigned int i=get_global_id(0);

    /*if(sizeof(struct obj_g_descriptor)!=48)
    {
        return;
    }*/



    //while(o_id<*num)
    for(o_id=0; o_id!=*num; o_id++)
    {
        if(i>=desc[o_id].start && i<desc[o_id].start + desc[o_id].tri_num)
        {
            break;
        }
    }

    //o_id=5;



    if(o_id!=*num)
    {
        __global struct triangle *cur_tri=&triangles[i]; ///shared memory?

        float4 rotpoints[3];
        rotpoints[0]=rot(cur_tri->vertices[0].pos, *pc_pos, *pc_rot);
        rotpoints[1]=rot(cur_tri->vertices[1].pos, *pc_pos, *pc_rot);
        rotpoints[2]=rot(cur_tri->vertices[2].pos, *pc_pos, *pc_rot);
        //-Xptxas -dlcm=cg

        for(int j=0; j<3; j++)
        {
            float rx;
            rx=rotpoints[j].x * (700.0f/rotpoints[j].z);
            float ry;
            ry=rotpoints[j].y * (700.0f/rotpoints[j].z);

            rx+=width/2.0f;
            ry+=height/2.0f;


            rotpoints[j].x=rx;
            rotpoints[j].y=ry;
            rotpoints[j].z=dcalc(rotpoints[j].z);
        }




        int y1;
        y1 = round(rotpoints[0].y);
        int y2;
        y2 = round(rotpoints[1].y);
        int y3;
        y3 = round(rotpoints[2].y);

        int x1;
        x1 = round(rotpoints[0].x);
        int x2;
        x2 = round(rotpoints[1].x);
        int x3;
        x3 = round(rotpoints[2].x);

        int miny;
        miny=min3(y1, y2, y3)-1; ///oh, wow
        int maxy;
        maxy=max3(y1, y2, y3);
        int minx;
        minx=min3(x1, x2, x3)-1;
        int maxx;
        maxx=max3(x1, x2, x3);



        if(out_of_bounds(miny, 0, height))
        {
            miny=max(miny, 0);
            miny=min(miny, (int)height);
        }
        if(out_of_bounds(maxy, 0, height))
        {
            maxy=max(maxy, 0);
            maxy=min(maxy, (int)height);
        }
        if(out_of_bounds(minx, 0, width))
        {
            minx=max(minx, 0);
            minx=min(minx, (int)width);
        }
        if(out_of_bounds(maxx, 0, width))
        {
            maxx=max(maxx, 0);
            maxx=min(maxx, (int)width);
        }

        if(abs(miny-maxy)>50 || abs(minx-maxx)>50)
        {
            return;
        }


        int rconstant=(x2*y3+x1*(y2-y3)-x3*y2+(x3-x2)*y1);
        //float rconstant2=1.0f/rconstant;
        float area=form(x1, y1, x2, y2, x3, y3, 0, 0, 0); ///add 1 to area to get point cloud!

        if(rconstant==0)
        {
            return;
        }

        for(int y=miny; y<maxy; y++)
        {
            for(int x=minx; x<maxx; x++)
            {


                //if(!out_of_bounds(x, minx, maxx) && !out_of_bounds(y, miny, maxy))
                {
                        float s1=fabs(((x2*y-x*y2)+(x3*y2-x2*y3)+(x*y3-x3*y))/2.0) + fabs(((x*y1-x1*y)+(x3*y-x*y3)+(x1*y3-x3*y1))/2.0) + fabs(((x2*y1-x1*y2)+(x*y2-x2*y)+(x1*y-x*y1))/2.0);



                        /*if(x>=width)
                        {
                            continue;
                        }
                        if(y>=height)
                        {
                            continue;
                        }
                        if(x<0)
                        {
                            continue;
                        }
                        if(y<0)
                        {
                            continue;
                        }*/


                        if(s1 < area + 0.001 && s1 > area - 0.001) /// DO NOT USE 0.0001 THIS CAUSES HOELELELELEL
                        {


                            volatile __global uint *ft=&depth_buffer [(int)y*width + (int)x];
                            //volatile __global uint *fi=&id_buffer    [(int)y*width + (int)x];
                            volatile __global float4 *fn=&normal_map [(int)y*width + (int)x];
                            volatile __global uint4 *ftm=&texture_map[(int)y*width + (int)x];



                            int2 coord;
                            coord.x=x;
                            coord.y=y;



                            float f1=rotpoints[0].z;
                            float f2=rotpoints[1].z;
                            float f3=rotpoints[2].z;



                            //float A=native_divide((f2*y3+f1*(y2-y3)-f3*y2+(f3-f2)*y1),rconstant);
                            //float B=native_divide(-(f2*x3+f1*(x2-x3)-f3*x2+(f3-f2)*x1),rconstant);
                            //float C=f1-A*x1 - B*y1;

                            /*float A=native_divide((f2*y3+f1*(y2-y3)-f3*y2+(f3-f2)*y1),rconstant);
                            float B=native_divide(-(f2*x3+f1*(x2-x3)-f3*x2+(f3-f2)*x1),rconstant);
                            float C=f1-A*x1 - B*y1;

                            float cdepth=(A*x + B*y + C);*/

                            float cdepth=interpolate(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);

                            if(cdepth<depth_cutoff) //use ldepth
                            {
                                continue;
                            }




                            f1=cur_tri->vertices[0].normal.x;
                            f2=cur_tri->vertices[1].normal.x;
                            f3=cur_tri->vertices[2].normal.x;

                            float inormalx=interpolate(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);


                            f1=cur_tri->vertices[0].normal.y;
                            f2=cur_tri->vertices[1].normal.y;
                            f3=cur_tri->vertices[2].normal.y;


                            float inormaly=interpolate(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);


                            f1=cur_tri->vertices[0].normal.z;
                            f2=cur_tri->vertices[1].normal.z;
                            f3=cur_tri->vertices[2].normal.z;

                            float inormalz=interpolate(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);

                            float4 inormal={inormalx, inormaly, inormalz, 0};



                            f1=1.0f/rotpoints[0].z;
                            f2=1.0f/rotpoints[1].z;
                            f3=1.0f/rotpoints[2].z;

                            float ldepth=interpolate(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);


                            f1=cur_tri->vertices[0].vt.x/rotpoints[0].z;
                            f2=cur_tri->vertices[1].vt.x/rotpoints[1].z;
                            f3=cur_tri->vertices[2].vt.x/rotpoints[2].z;

                            float vtx=interpolate(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);




                            f1=cur_tri->vertices[0].vt.y/rotpoints[0].z;
                            f2=cur_tri->vertices[1].vt.y/rotpoints[1].z;
                            f3=cur_tri->vertices[2].vt.y/rotpoints[2].z;

                            float vty=interpolate(f1, f2, f3, x, y, x1, x2, x3, y1, y2, y3, rconstant);



                            //uint4 vt={(vtx/ldepth)*mulint, (vty/ldepth)*mulint, o_id, 0};
                            uint4 vt={(vtx/ldepth)*mulint, (vty/ldepth)*mulint, o_id, i};
                            //uint4 vt={0, 0, 0, 0};
                            ///texture coords are between 0 and 1
                            ///?



                            uint mydepth=floor(cdepth*(mulint));///dont need interpolated depth yet



                            //uint4 tmap;

                            if(mydepth>0 && mydepth<*ft) ///have two buffers, write to both, extract most correct information in second kernel
                            {
                                //while(*ft!=mydepth)
                                {
                                   // if(mydepth<*ft)
                                    {
                                        *ft=mydepth;
                                        //*fn=inormal;
                                        //*ftm=vt;
                                        //t=*fn;
                                        // *fi=i;
                                        //arr=*ftm;
                                    }
                                    //else
                                    {
                                    //    break;
                                    }
                                }
                            }

                            //if(mydepth>0 && mydepth<*ft)
                            /*if(mydepth!=0)
                            {

                                uint idbits = IDENTIFICATION_BITS;
                                uint idbitshex=0x3FF;
                                ///id is i

                                uint idshiftidbits = idbitshex & (i >> (32-idbits)); ///leaves idbits bits alive, most significant bits
                                uint leftdepth = mydepth << idbits;
                                uint leftblank = leftdepth & (idbitshex ^ 0xFFFFFFFF);

                                uint mdepth=leftblank ^ idshiftidbits;


                                uint ibdepth=atomic_min(ft, mdepth);
                                if(mdepth < ibdepth)
                                {
                                    //*fn=inormal;
                                    *ftm=vt;
                                }

                            }*/

                            /*if(mydepth!=0)
                            {
                                float ibdepth=atomic_min(ft, mydepth);
                                if(mydepth < ibdepth)
                                {
                                    //*fn=inormal;
                                    *ftm=vt;
                                }
                            }*/
                        }
                }
            }
        }
    }
}
