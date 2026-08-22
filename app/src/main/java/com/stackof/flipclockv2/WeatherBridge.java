package com.stackof.flipclockv2;

/**
 * JNI bridge to pass current weather data to the native SDL rendering layer.
 * <p>
 * The native function updates internal strings that are drawn by
 * {@code weather_overlay.c} when burn-in protection reaches its peak offset.
 */
public class WeatherBridge {
    /**
     * Update the weather text shown by the native overlay.
     *
     * @param location    Human-readable location name, e.g. "北京市".
     * @param temperature Temperature string, e.g. "24°C".
     * @param description Short weather description, e.g. "晴".
     */
    public static native void nativeUpdateWeather(
            String location, String temperature, String description);
}
