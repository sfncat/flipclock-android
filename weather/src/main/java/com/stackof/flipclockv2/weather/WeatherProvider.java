package com.stackof.flipclockv2.weather;

import java.util.List;

/**
 * Pluggable provider for weather data.
 */
public interface WeatherProvider {
    /**
     * Search locations matching the given name.
     *
     * @param name     User-supplied location name, e.g. "北京".
     * @param language Language code for result names, e.g. "zh" or "en".
     * @return List of candidate locations, ordered by relevance.
     * @throws WeatherException if the request fails or returns no results.
     */
    List<GeoLocation> searchLocations(String name, String language)
            throws WeatherException;

    /**
     * Fetch current weather for the given location.
     *
     * @param location Location previously returned by {@link #searchLocations}.
     * @return Current weather information.
     * @throws WeatherException if the request fails or cannot be parsed.
     */
    WeatherInfo fetchWeather(GeoLocation location) throws WeatherException;
}
