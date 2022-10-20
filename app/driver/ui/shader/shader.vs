#version 150

//背景相关
in vec4 vertexIn; 
in vec2 textureIn; 
out vec2 textureOut;

//标记相关
in vec4 vertexSign;
in vec4 colorSign;
uniform mat4 matMVP;
uniform float widthPercent;
uniform float heightPercent;
out float outIsModel;
out vec4 outColorSign;

//用于表示此顶点是否为model顶点
uniform float isModel;

void main(void){             
    if(isModel == 0){
        gl_Position = vertexIn; 
        textureOut = textureIn; 
    }else{
        //处理模型
        gl_Position = matMVP * vertexSign;
        gl_Position.x *= widthPercent;
        gl_Position.y *= heightPercent;
        outColorSign = colorSign;
    }        
    outIsModel = isModel;
}