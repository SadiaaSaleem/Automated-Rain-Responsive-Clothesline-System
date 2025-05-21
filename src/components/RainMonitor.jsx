import { useState, useEffect, useRef } from 'react';
import { Box, Card, Typography, CircularProgress, LinearProgress, Divider } from '@mui/material';
import axios from 'axios';

const BACKEND_URL = 'http://localhost:4000/status';
const PREDICTION_URL = 'http://localhost:4000/ai-predict';

const RainMonitor = () => {
  const [rainData, setRainData] = useState(null);
  const [prediction, setPrediction] = useState(null);
  const [loading, setLoading] = useState(true);
  const predictionIntervalRef = useRef(null);

  // Function to fetch prediction
  const fetchPrediction = async () => {
    try {
      const predictionRes = await axios.post(PREDICTION_URL);
      setPrediction(predictionRes.data);
    } catch (err) {
      console.error('Error fetching prediction:', err);
    }
  };

  useEffect(() => {
    let isMounted = true;
    const fetchData = async () => {
      try {
        const res = await axios.get(BACKEND_URL);
        const data = res.data;
        if (!data.isRaining) {
          // Reset values when rain stops
          setRainData({
            ...data,
            intensity: 0,
            duration: '0s',
          });
          setPrediction(null); // Clear prediction
          // Stop prediction interval if running
          if (predictionIntervalRef.current) {
            clearInterval(predictionIntervalRef.current);
            predictionIntervalRef.current = null;
          }
        } else {
          setRainData(data);
          // Start prediction interval if not already running
          if (!predictionIntervalRef.current) {
            fetchPrediction(); // Fetch immediately
            predictionIntervalRef.current = setInterval(fetchPrediction, 5000);
          }
        }
        if (isMounted) setLoading(false);
      } catch (err) {
        console.error('Error fetching data:', err);
        if (isMounted) setLoading(false);
      }
    };

    // Initial fetch
    fetchData();
    const dataInterval = setInterval(fetchData, 2000); // Update data every 2 seconds

    // Cleanup intervals
    return () => {
      isMounted = false;
      clearInterval(dataInterval);
      if (predictionIntervalRef.current) {
        clearInterval(predictionIntervalRef.current);
        predictionIntervalRef.current = null;
      }
    };
  }, []);

  if (loading) {
    return (
      <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', minHeight: '100vh' }}>
        <CircularProgress />
      </Box>
    );
  }

  return (
    <Box
      sx={{
        display: 'flex',
        justifyContent: 'center',
        alignItems: 'center',
        minHeight: '100vh',
        backgroundColor: '#f5f5f5',
      }}
    >
      <Card
        sx={{
          p: 4,
          maxWidth: 400,
          width: '100%',
          textAlign: 'center',
          boxShadow: 3,
        }}
      >
        <Typography variant="h4" gutterBottom>
          Rain Monitor
        </Typography>
        
        {rainData && (
          <>
            {/* Basic Status */}
            <Box sx={{ mb: 3 }}>
              <Typography 
                variant="h5" 
                sx={{ 
                  color: rainData.isRaining ? '#1976d2' : '#2e7d32',
                  fontWeight: 'bold'
                }}
              >
                Status: {rainData.isRaining ? 'Raining' : 'Dry'}
              </Typography>
            </Box>

            <Divider sx={{ my: 2 }} />
            
            {/* Detailed Data */}
            <Box sx={{ mb: 2 }}>
              <Typography variant="h6" gutterBottom>
                Rain Details
              </Typography>
              
              <Box sx={{ mb: 2 }}>
                <Typography variant="subtitle1">Current Intensity</Typography>
                <LinearProgress 
                  variant="determinate" 
                  value={rainData.intensity} 
                  sx={{ 
                    height: 10, 
                    borderRadius: 5, 
                    mb: 1,
                    backgroundColor: '#e0e0e0',
                    '& .MuiLinearProgress-bar': {
                      backgroundColor: '#1976d2'
                    }
                  }}
                />
                <Typography>{rainData.intensity}%</Typography>
              </Box>
              
              <Box sx={{ mb: 2 }}>
                <Typography variant="subtitle1">Current Duration</Typography>
                <Typography>{rainData.duration}</Typography>
              </Box>
            </Box>

            <Divider sx={{ my: 2 }} />
            
            {/* Prediction */}
            {prediction && (
              <Box sx={{ mt: 2, p: 2, bgcolor: '#f8f9fa', borderRadius: 2 }}>
                <Typography variant="h6" color="primary" gutterBottom>
                  Rain Prediction
                </Typography>
                <Box sx={{ mb: 2 }}>
                  <Typography variant="subtitle1" sx={{ fontWeight: 'bold' }}>
                    Predicted Rain Duration:
                  </Typography>
                  <Typography variant="h5" color="primary" sx={{ mt: 1 }}>
                    {prediction.predicted_remaining_minutes} minutes
                  </Typography>
                </Box>
                <Typography variant="subtitle1" sx={{ mt: 1 }}>
                  Confidence: {prediction.confidence}
                </Typography>
              </Box>
            )}
          </>
        )}
      </Card>
    </Box>
  );
};

export default RainMonitor; 