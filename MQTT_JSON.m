myMQTT = mqttclient('tcp://mqtt.eclipseprojects.io', Port = 1883);
Topic_sub = "Test";

filePath = 'C:\Users\pc\esp\MQTT_TEST\MQTT2FIREBASE.py';

% Tự động gọi và chạy file thông qua cmd
command = ['cmd /c start "" python "', filePath, '"'];
status = system(command);

% Kiểm tra xem việc gọi lệnh có thành công hay không
if status == 0
    disp('Command executed successfully.');
else
    disp('Error executing command.');
end

x=0;
y=0;
z=0;

data = struct('x', x, 'y', y, 'z', z);

Data = subscribe(myMQTT,Topic_sub, QualityOfService = 1, Callback = @(src, msg) handleMessage(src, msg, data));

function handleMessage(~, message, data)
    try
        op = jsondecode(message);
        disp(op);
        ax = op.ax;
        ay = op.ay;
        az = op.az;

        data.x = [data.x, ax];
        data.y = [data.y, ay];
        data.z = [data.z, az];

        subplot(3,1,1);
        plot(data.x);
        ylim([0,5000]);
        grid ON;
        title("Luminosity");

        subplot(3,1,2);
        plot(data.y);
        ylim([0,5000]);
        grid ON;
        title("Humidity");

        subplot(3,1,3);
        plot(data.z);
        ylim([0,5000]);
        grid ON;
        title("Accelerator");
        drawnow     

    catch ME
        disp(['Lỗi khi xử lý tin nhắn: ' ME.message]);
    end
end