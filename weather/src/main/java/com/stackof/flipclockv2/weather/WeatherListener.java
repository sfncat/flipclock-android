package com.stackof.flipclockv2.weather;

/**
 * Listener for asynchronous weather updates from {@link WeatherManager}.
 * <p>
 * Callbacks are invoked on the library's internal background thread.
 * Callers that need to update UI should post to the main thread themselves.
 */
public interface WeatherListener {
    /**
     * Called when weather data is successfully updated.
     *
     * @param info The latest weather information.
     */
    void onWeatherUpdated(WeatherInfo info);

    /**
     * Called when a weather request fails.
     *
     * @param error The exception describing the failure.
     */
    void onError(Throwable error);
}
