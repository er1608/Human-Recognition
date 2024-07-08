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
        tic;
        op = jsondecode(message);
        disp(op);
        ax = op.ax;
        ay = op.ay;
        az = op.az;

        % Thêm dữ liệu mới vào data
        data.x = [data.x, ax];
        data.y = [data.y, ay];
        data.z = [data.z, az];

        subplot(3,1,[1 2]);
        plot(data.x, 'r'); % Vẽ đồ thị cho data.x với màu đỏ
        hold on;
        plot(data.y, 'g'); % Vẽ đồ thị cho data.y với màu xanh lá cây
        plot(data.z, 'b'); % Vẽ đồ thị cho data.z với màu xanh dương
        hold off;
        ylim([0,20000]);
        grid on;
        title('Human Recognition');
        legend('Ax', 'Ay', 'Az');

        elapsedTime = toc;
        disp(['Thời gian chạy của callback là: ', num2str(elapsedTime), ' giây']);
    catch ME
        disp(['Lỗi khi xử lý tin nhắn: ' ME.message]);
    end
end