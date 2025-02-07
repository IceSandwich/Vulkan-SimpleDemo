#version 450
// location指明framebuffer的索引
layout(location = 0) in vec3 fragColor; //只指定location的in变量由PipelineVertexInputStateCreateInfo确定，属于descriptors
layout(location = 1) in vec2 fragTexCoord; 

// 这里set=0对应descriptorSetLayout的索引，不是descriptorSets的索引
//layout(binding = 1, set = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor; //只指定location的out变量由subpass.attachments确定，属于attachments
//输出的变量只指定location的话，默认是subpass.pColorAttachments[location]的附件

void main() {
    outColor = vec4(fragTexCoord, 0.0, 1.0);
    //outColor = texture(texSampler, fragTexCoord);
}