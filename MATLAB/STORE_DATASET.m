myMQTT = mqttclient('tcp://mqtt.eclipseprojects.io', Port = 1883);
Topic_sub = "Test";

filePath = 'C:\Users\pc\esp\MQTT_TEST\MQTT2FIREBASE.py';

command = ['cmd /c start "" python "', filePath, '"'];
status = system(command);

if status == 0
    disp('Command executed successfully.');
else
    disp('Error executing command.');
end

Data = subscribe(myMQTT,Topic_sub, QualityOfService = 1, Callback = @(src, msg) handleMessage(src, msg));

function handleMessage(~, message)
    try
        message_str = jsonencode(message);
        % Tách chuỗi JSON thành cấu trúc dữ liệu
        data_cell = strsplit(message_str, ',');

        data_cell{1} = [];  
        data_cell = data_cell(~cellfun('isempty',data_cell));
        processed_str = strjoin(data_cell, ',');
        processed_str = strrep(processed_str, '{', '');  
        processed_str = strrep(processed_str, '}', '');  
        processed_str = strrep(processed_str, '"', '');  
        processed_str = strtrim(processed_str);
        disp(processed_str);
        fid = fopen('C:\Users\pc\OneDrive\Documents\Tài Liệu + Thực hành\NCKH\Dataset\data_1651.txt', 'a');  % Mở file để ghi vào cuối
        fprintf(fid, '%s\n', processed_str);  % Ghi message_str vào file, mỗi message trên một dòng
        fclose(fid);  % Đóng file sau khi ghi

    catch ME
        disp(['Lỗi khi xử lý tin nhắn: ' ME.message]);
    end
end
