#version 450

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo; //属于descriptor，由PipelineLayout确定

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
//只指定location的in变量由PipelineVertexInputStateCreateInfo确定，属于descriptors
//这些in变量只指定location，其实他们的binding不一定一样的，比如说：inPosition, inColor, inTexCoord都是binding=0，但是
//layout(location=3) in vec3 inInstanceDataPos;
//layout(location=4) in vec3 inInstanceDataColor;
//上面两个其实是binding=1，但是location不能从0开始算，location他们都是共享的，不能重叠。可以说，location给定每个descriptor唯一id
//binding只是分组，便于在使用时对对应的数据绑定，例如
//使用的时候：
//cmd.bindVertexBuffers(0, vertexBuffers, offsets); // binding=0的数据
//cmd.bindxxxBuffers(1, xxx, offsets); // binding=1的数据


layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

/*
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);
*/

void main() {
    //gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    //fragColor = colors[gl_VertexIndex];

    //gl_Position = vec4(inPosition, 0.0, 1.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}