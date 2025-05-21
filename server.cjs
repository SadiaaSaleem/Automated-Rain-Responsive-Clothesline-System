const mqtt = require('mqtt');
const express = require('express');
const cors = require('cors');
const axios = require('axios');

const MQTT_HOST = 'a1844d6db9f847b88e8653fc92321028.s1.eu.hivemq.cloud';
const MQTT_PORT = 8883;
const MQTT_USERNAME = 'Device0001';
const MQTT_PASSWORD = 'Device0001';
const MQTT_TOPICS = ['RainSensorData', 'RainSensorData/aiData'];

let latestStatus = {
  isRaining: false,
  intensity: 0,
  duration: '0s',
  averageIntensity: 0,
  startTime: null,
  readings: []
};

// MQTT connection options
const options = {
  host: MQTT_HOST,
  port: MQTT_PORT,
  protocol: 'mqtts',
  username: MQTT_USERNAME,
  password: MQTT_PASSWORD,
};

const client = mqtt.connect(options);
//const client = mqtt.connect('mqtt://broker.hivemq.com:1883');
client.on('connect', () => {
  console.log('Connected to HiveMQ Cloud via MQTT');
  MQTT_TOPICS.forEach(topic => {
    client.subscribe(topic, (err) => {
      if (err) {
        console.error(`Failed to subscribe to ${topic}:`, err);
      } else {
        console.log('Subscribed to', topic);
      }
    });
  });
});

client.on('message', (topic, message) => {
  try {
    const data = message.toString();
    console.log(`Received on ${topic}:`, data);
    
    if (topic === 'RainSensorData') {
      // Basic rain status
      latestStatus.isRaining = data === 'Raining';
      if (!latestStatus.isRaining) {
        // Reset readings when rain stops
        latestStatus.readings = [];
        latestStatus.startTime = null;
        latestStatus.averageIntensity = 0;
      }
    } 
    else if (topic === 'RainSensorData/aiData') {
      // Parse the detailed rain data
      const [intensityPart, durationPart] = data.split(', ');
      
      // Extract intensity value (remove % and convert to number)
      const intensity = parseFloat(intensityPart.split(': ')[1].replace('%', ''));
      
      // Extract duration
      const duration = durationPart.split(': ')[1];
      
      // Update latest status
      latestStatus.intensity = intensity;
      latestStatus.duration = duration;
      
      // If this is the first reading of a rain event
      if (latestStatus.isRaining && !latestStatus.startTime) {
        latestStatus.startTime = new Date();
        latestStatus.readings = [];
      }
      
      // Add reading to history if it's raining
      if (latestStatus.isRaining) {
        latestStatus.readings.push({
          intensity,
          timestamp: new Date()
        });
        
        // Calculate average intensity
        const totalIntensity = latestStatus.readings.reduce((sum, reading) => sum + reading.intensity, 0);
        latestStatus.averageIntensity = totalIntensity / latestStatus.readings.length;
      }
    }
    
    console.log('Updated status:', latestStatus);
  } catch (error) {
    console.error('Error processing message:', error);
  }
});

client.on('error', (err) => {
  console.error('MQTT error:', err);
});

// Express server
const app = express();
app.use(cors());
app.use(express.json());

app.get('/status', (req, res) => {
  res.json(latestStatus);
});

app.post('/ai-predict', async (req, res) => {
  try {
    // Forward the request body to the Python Flask API
    const response = await axios.post('http://localhost:5000/ai-predict', {
      rain_intensity: latestStatus.intensity, 
      duration: latestStatus.duration
    });
    res.json(response.data);
  } catch (error) {
    console.error('Error calling Python AI API:', error.message);
    res.status(500).json({ error: 'Failed to get AI prediction' });
  }
});

const PORT = 4000;
app.listen(PORT, () => {
  console.log(`Backend server running on http://localhost:${PORT}`);
}); 