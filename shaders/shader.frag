#version 450
// locationָ��framebuffer������
layout(location = 0) in vec3 fragColor; //ָֻ��location��in������PipelineVertexInputStateCreateInfoȷ��������descriptors
layout(location = 1) in vec2 fragTexCoord; 

// ����set=0��ӦdescriptorSetLayout������������descriptorSets������
//layout(binding = 1, set = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor; //ָֻ��location��out������subpass.attachmentsȷ��������attachments
//����ı���ָֻ��location�Ļ���Ĭ����subpass.pColorAttachments[location]�ĸ���

void main() {
    outColor = vec4(fragTexCoord, 0.0, 1.0);
    //outColor = texture(texSampler, fragTexCoord);
}