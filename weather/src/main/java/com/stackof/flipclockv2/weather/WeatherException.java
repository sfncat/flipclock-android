package com.stackof.flipclockv2.weather;

/**
 * Exception thrown when weather data cannot be fetched or parsed.
 */
public class WeatherException extends Exception {
    public WeatherException(String message) {
        super(message);
    }

    public WeatherException(String message, Throwable cause) {
        super(message, cause);
    }
}
