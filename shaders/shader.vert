#version 450

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo; //����descriptor����PipelineLayoutȷ��

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
//ָֻ��location��in������PipelineVertexInputStateCreateInfoȷ��������descriptors
//��Щin����ָֻ��location����ʵ���ǵ�binding��һ��һ���ģ�����˵��inPosition, inColor, inTexCoord����binding=0������
//layout(location=3) in vec3 inInstanceDataPos;
//layout(location=4) in vec3 inInstanceDataColor;
//����������ʵ��binding=1������location���ܴ�0��ʼ�㣬location���Ƕ��ǹ���ģ������ص�������˵��location����ÿ��descriptorΨһid
//bindingֻ�Ƿ��飬������ʹ��ʱ�Զ�Ӧ�����ݰ󶨣�����
//ʹ�õ�ʱ��
//cmd.bindVertexBuffers(0, vertexBuffers, offsets); // binding=0������
//cmd.bindxxxBuffers(1, xxx, offsets); // binding=1������


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