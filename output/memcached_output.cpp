#include "memcached_output.hpp"

MemcachedOutput::MemcachedOutput(VideoOptions const *options) : Output(options)
{
	opt = options;

	prepareMemcachedConnection();
    testMemcachedConnection();
    prepareRedisConnection();
    testRedisConnection();
}

MemcachedOutput::~MemcachedOutput()
{	
	memcached_free(memc);
	memcached_server_list_free(servers);
	freeReplyObject(reply);
    redisFree(redis);
}

void MemcachedOutput::prepareMemcachedConnection() {
    // Ensure memc is null before creating a new instance
    if (memc) {
        memcached_free(memc);
        memc = nullptr;
    }

    // Create a new memcached instance
    memc = memcached_create(nullptr);
    if (!memc) {
        LOG_ERROR("Failed to create memcached instance");
        return;
    }

    // Use a UNIX socket if specified, otherwise parse hostname:port
    if (opt->memcached.front() == '/') {  
        // Connecting via UNIX socket
        memcached_return_t rc = memcached_server_add_unix_socket(memc, opt->memcached.c_str());
        if (rc != MEMCACHED_SUCCESS) {
            LOG_ERROR("Failed to connect to Memcached via UNIX socket: " << memcached_strerror(memc, rc));
            memcached_free(memc);
            memc = nullptr;
            return;
        }
    } else {
        // Connecting via hostname:port
        size_t colon_pos = opt->memcached.find(':');
        if (colon_pos == std::string::npos) {
            LOG_ERROR("Invalid format: opt->memcached should be in 'hostname:port' format or a UNIX socket path.");
            memcached_free(memc);
            memc = nullptr;
            return;
        }

        std::string hostname = opt->memcached.substr(0, colon_pos);
        int port;
        try {
            port = std::stoi(opt->memcached.substr(colon_pos + 1));
        } catch (const std::exception &e) {
            LOG_ERROR("Invalid port number in opt->memcached: " << e.what());
            memcached_free(memc);
            memc = nullptr;
            return;
        }

        memcached_return_t rc = memcached_server_add(memc, hostname.c_str(), port);
        if (rc != MEMCACHED_SUCCESS) {
            LOG_ERROR("Failed to connect to Memcached server: " << memcached_strerror(memc, rc));
            memcached_free(memc);
            memc = nullptr;
            return;
        }

        // Append server to server list
        servers = memcached_server_list_append(nullptr, hostname.c_str(), port, &rc);
        if (rc != MEMCACHED_SUCCESS) {
            LOG_ERROR("Failed to create server list: " << memcached_strerror(memc, rc));
            memcached_free(memc);
            memc = nullptr;
            return;
        }

        // Push server list to memcached instance
        rc = memcached_server_push(memc, servers);
        memcached_server_list_free(servers); // Free server list after pushing
        if (rc != MEMCACHED_SUCCESS) {
            LOG_ERROR("Failed to push server list to Memcached: " << memcached_strerror(memc, rc));
            memcached_free(memc);
            memc = nullptr;
            return;
        }
    }

    // Set the binary protocol behavior
    memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
}

void MemcachedOutput::testMemcachedConnection() {
    const char *key = "my_key";
    const char *value = "Hello, Memcached!";
    size_t key_len = strlen(key);
    size_t value_length = strlen(value);
    rc = memcached_set(memc, key, key_len, value, value_length, 0, 0);

    // Retrieve the value from Memcached
    size_t returned_value_length;
    uint32_t returned_flags;
    char *returned_value = memcached_get(memc, key, strlen(key),
                                         &returned_value_length, &returned_flags, &rc);
    if (rc == MEMCACHED_SUCCESS) {
        // Assert that the retrieved value matches the expected value
        assert(returned_value_length == value_length);
        assert(std::memcmp(returned_value, value, value_length) == 0);

        std::cout << "Retrieved value from Memcached matches expected value" << std::endl;
        std::cout << "Value: " << returned_value << std::endl;

        // Free the memory allocated by libmemcached
        free(returned_value);
    } else {
        LOG_ERROR("Failed to retrieve value from Memcached: ");
    }
}

void MemcachedOutput::prepareRedisConnection() {
    // Prepare redis connect string
    size_t colon_pos = opt->redis.find(':');
    if (colon_pos == std::string::npos) {
        LOG_ERROR("Invalid format: opt->redis should be in 'hostname:port' format");
        return;
    }
    std::string hostname = opt->redis.substr(0, colon_pos);
    int port = std::stoi(opt->redis.substr(colon_pos + 1));

    // Connect to redis
    redis = redisConnect(hostname.c_str(), port);
    if (redis == NULL || redis->err) {
        if (redis) {
            LOG_ERROR("Error redis");
        } else {
            LOG_ERROR("Can't allocate redis context");
        }
        exit(1);
    }
}

void MemcachedOutput::testRedisConnection() {
    // Test redis
    redisReply *reply = (redisReply *)redisCommand(redis, "SET CameraService Alive");
    if (reply == nullptr) {
        LOG_ERROR("Failed to set value in Redis");
    } else {
        freeReplyObject(reply);
    }
}

void MemcachedOutput::outputBuffer(void *mem, size_t size, int64_t /*timestamp_us*/, uint32_t /*flags*/)
{
    // Convert raw memory buffer to OpenCV Mat
    cv::Mat img(opt->height, opt->width, CV_8UC1, mem);

    // Encode the image in BMP format
    std::vector<uchar> bmp_data;
    if (!cv::imencode(".bmp", img, bmp_data)) {
        LOG_ERROR("Failed to encode image to BMP format");
        return;
    }

    // Generate timestamp
    int64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    char timestamp[16];
    sprintf(timestamp, "%li", t);

    // Store the BMP-encoded image in Memcached
    memcached_return_t rc = memcached_set(memc, timestamp, strlen(timestamp), 
                                          reinterpret_cast<char*>(bmp_data.data()), bmp_data.size(), 
                                          (time_t)0, (uint32_t)16);

    if (rc == MEMCACHED_SUCCESS) {
        LOG(2, "Value added successfully to memcached: " << timestamp);
    } else {
        LOG_ERROR("Error: " << rc << " adding value to memcached " << timestamp);
    }

    // Build Redis command
    std::string time_str = timestamp;
    std::string redis_command = "XADD Bitmaps ";
    redis_command += "MAXLEN ~ 1000 "; // Stream max length of 1000 entries
    redis_command += time_str + " "; // Custom id matching the memcached entry
    redis_command += "memcached " + time_str + " ";
    redis_command += "sensor_id Libcamera ";
    redis_command += "joint_key -1 ";
    redis_command += "track_id -1 ";
    redis_command += "start_time -1 ";
    redis_command += "feedback_id -1 ";
    redis_command += "width " + std::to_string(opt->width) + " ";
    redis_command += "height " + std::to_string(opt->height) + " ";
    redis_command += "gain " + std::to_string(opt->gain) + " ";
    redis_command += "roi " + opt->roi;
    // TODO fix type conversions
	// redis_command += "framerate " + opt->framerate + " ";
	// redis_command += "shutter " + opt->shutter + " ";

    // Execute Redis command
    redisReply *reply = (redisReply *)redisCommand(redis, redis_command.c_str());

    if (reply == NULL) {
        LOG_ERROR("Failed to execute command");
    } else if (reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR("Redis reply error");
    } else {
        LOG(2, "Entry added to redis");
    }
}
