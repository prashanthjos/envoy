package io.envoyproxy.envoymobile

import io.envoyproxy.envoymobile.engine.EnvoyConfiguration
import io.envoyproxy.envoymobile.engine.EnvoyConfiguration.TrustChainVerification
import io.envoyproxy.envoymobile.engine.EnvoyEngine
import io.envoyproxy.envoymobile.engine.EnvoyEngineImpl
import io.envoyproxy.envoymobile.engine.EnvoyNativeFilterConfig
import io.envoyproxy.envoymobile.engine.types.EnvoyHTTPFilterFactory
import io.envoyproxy.envoymobile.engine.types.EnvoyKeyValueStore
import io.envoyproxy.envoymobile.engine.types.EnvoyStringAccessor
import java.util.UUID

/** Builder used for creating and running a new `Engine` instance. */
open class EngineBuilder() {
  protected var onEngineRunning: (() -> Unit) = {}
  protected var logger: ((LogLevel, String) -> Unit)? = null
  protected var eventTracker: ((Map<String, String>) -> Unit)? = null
  protected var enableProxying = false
  private var runtimeGuards = mutableMapOf<String, Boolean>()
  private var engineType: () -> EnvoyEngine = {
    EnvoyEngineImpl(
      onEngineRunning,
      { level, msg -> logger?.let { it(LogLevel.from(level), msg) } },
      eventTracker,
      disableDnsRefreshOnNetworkChange
    )
  }
  private var logLevel = LogLevel.INFO
  private var connectTimeoutSeconds = 10
  private var disableDnsRefreshOnFailure = false
  private var disableDnsRefreshOnNetworkChange = false
  private var dnsRefreshSeconds = 60
  private var dnsFailureRefreshSecondsBase = 2
  private var dnsFailureRefreshSecondsMax = 10
  private var dnsQueryTimeoutSeconds = 5
  private var dnsMinRefreshSeconds = 60
  private var dnsPreresolveHostnames = listOf<String>()
  private var enableDNSCache = false
  private var dnsCacheSaveIntervalSeconds = 1
  // null means the DNS resolver will try indefinitely until it succeeds.
  private var dnsNumRetries: Int? = null
  private var enableDrainPostDnsRefresh = false
  internal var enableHttp3 = true
  private var http3ConnectionOptions = ""
  private var http3ClientConnectionOptions = ""
  private var quicHints = mutableMapOf<String, Int>()
  private var quicCanonicalSuffixes = mutableListOf<String>()
  private var enableGzipDecompression = true
  private var enableBrotliDecompression = false
  private var numTimeoutsToTriggerPortMigration = 0
  private var enableSocketTagging = false
  private var enableInterfaceBinding = false
  private var h2ConnectionKeepaliveIdleIntervalMilliseconds = 1
  private var h2ConnectionKeepaliveTimeoutSeconds = 10
  private var maxConnectionsPerHost = 7
  private var streamIdleTimeoutSeconds = 15
  private var perTryIdleTimeoutSeconds = 15
  private var appVersion = "unspecified"
  private var appId = "unspecified"
  private var trustChainVerification = TrustChainVerification.VERIFY_TRUST_CHAIN
  private var platformFilterChain = mutableListOf<EnvoyHTTPFilterFactory>()
  private var nativeFilterChain = mutableListOf<EnvoyNativeFilterConfig>()
  private var stringAccessors = mutableMapOf<String, EnvoyStringAccessor>()
  private var keyValueStores = mutableMapOf<String, EnvoyKeyValueStore>()
  private var enablePlatformCertificatesValidation = false
  private var upstreamTlsSni: String = ""
  private var h3ConnectionKeepaliveInitialIntervalMilliseconds = 0

  /**
   * Sets a log level to use with Envoy.
   *
   * @param logLevel the log level to use with Envoy.
   * @return this builder.
   */
  fun setLogLevel(logLevel: LogLevel): EngineBuilder {
    this.logLevel = logLevel
    return this
  }

  /**
   * Add a timeout for new network connections to hosts in the cluster.
   *
   * @param connectTimeoutSeconds timeout for new network connections to hosts in the cluster.
   * @return this builder.
   */
  fun addConnectTimeoutSeconds(connectTimeoutSeconds: Int): EngineBuilder {
    this.connectTimeoutSeconds = connectTimeoutSeconds
    return this
  }

  /** Disables DNS refresh on failure. */
  fun setDisableDnsRefreshOnFailure(disableDnsRefreshOnFailure: Boolean): EngineBuilder {
    this.disableDnsRefreshOnFailure = disableDnsRefreshOnFailure
    return this
  }

  /** Disables DNS refresh on network change. */
  fun setDisableDnsRefreshOnNetworkChange(
    disableDnsRefreshOnNetworkChange: Boolean
  ): EngineBuilder {
    this.disableDnsRefreshOnNetworkChange = disableDnsRefreshOnNetworkChange
    return this
  }

  /**
   * Add a default rate at which to refresh DNS.
   *
   * @param dnsRefreshSeconds default rate in seconds at which to refresh DNS.
   * @return this builder.
   */
  fun addDNSRefreshSeconds(dnsRefreshSeconds: Int): EngineBuilder {
    this.dnsRefreshSeconds = dnsRefreshSeconds
    return this
  }

  /**
   * Add a rate at which to refresh DNS in case of DNS failure.
   *
   * @param base rate in seconds.
   * @param max rate in seconds.
   * @return this builder.
   */
  fun addDNSFailureRefreshSeconds(base: Int, max: Int): EngineBuilder {
    this.dnsFailureRefreshSecondsBase = base
    this.dnsFailureRefreshSecondsMax = max
    return this
  }

  /**
   * Add a rate at which to timeout DNS queries.
   *
   * @param dnsQueryTimeoutSeconds rate in seconds to timeout DNS queries.
   * @return this builder.
   */
  fun addDNSQueryTimeoutSeconds(dnsQueryTimeoutSeconds: Int): EngineBuilder {
    this.dnsQueryTimeoutSeconds = dnsQueryTimeoutSeconds
    return this
  }

  /**
   * Add the minimum rate at which to refresh DNS. Once DNS has been resolved for a host, DNS TTL
   * will be respected, subject to this minimum. Defaults to 60 seconds.
   *
   * @param dnsMinRefreshSeconds minimum rate in seconds at which to refresh DNS.
   * @return this builder.
   */
  fun addDNSMinRefreshSeconds(dnsMinRefreshSeconds: Int): EngineBuilder {
    this.dnsMinRefreshSeconds = dnsMinRefreshSeconds
    return this
  }

  /**
   * Add a list of hostnames to preresolve on Engine startup.
   *
   * @param dnsPreresolveHostnames hostnames to preresolve.
   * @return this builder.
   */
  fun addDNSPreresolveHostnames(dnsPreresolveHostnames: List<String>): EngineBuilder {
    this.dnsPreresolveHostnames = dnsPreresolveHostnames
    return this
  }

  /**
   * Specifies the number of retries before the resolver gives up. If not specified, the resolver
   * will retry indefinitely until it succeeds or the DNS query times out.
   *
   * @param dnsNumRetries the number of retries
   * @return this builder
   */
  fun setDnsNumRetries(dnsNumRetries: Int): EngineBuilder {
    this.dnsNumRetries = dnsNumRetries
    return this
  }

  /**
   * Specify whether to drain connections after the resolution of a soft DNS refresh. A refresh may
   * be triggered directly via the Engine API, or as a result of a network status update provided by
   * the OS. Draining connections does not interrupt existing connections or requests, but will
   * establish new connections for any further requests.
   *
   * @param enableDrainPostDnsRefresh whether to drain connections after soft DNS refresh.
   * @return This builder.
   */
  fun enableDrainPostDnsRefresh(enableDrainPostDnsRefresh: Boolean): EngineBuilder {
    this.enableDrainPostDnsRefresh = enableDrainPostDnsRefresh
    return this
  }

  /**
   * Specify whether to enable DNS cache.
   *
   * Note that DNS cache requires an addition of a key value store named 'reserved.platform_store'.
   *
   * @param enableDNSCache whether to enable DNS cache. Disabled by default.
   * @param saveInterval the interval at which to save results to the configured key value store.
   * @return This builder.
   */
  fun enableDNSCache(enableDNSCache: Boolean, saveInterval: Int = 1): EngineBuilder {
    this.enableDNSCache = enableDNSCache
    this.dnsCacheSaveIntervalSeconds = saveInterval
    return this
  }

  /**
   * Specify whether to do gzip response decompression or not. Defaults to true.
   *
   * @param enableGzipDecompression whether or not to gunzip responses.
   * @return This builder.
   */
  fun enableGzipDecompression(enableGzipDecompression: Boolean): EngineBuilder {
    this.enableGzipDecompression = enableGzipDecompression
    return this
  }

  /**
   * Specify whether to enable HTTP3. Defaults to true.
   *
   * @param enableHttp3 whether or not to enable HTTP3.
   * @return This builder.
   */
  fun enableHttp3(enableHttp3: Boolean): EngineBuilder {
    this.enableHttp3 = enableHttp3
    return this
  }

  /**
   * Specify whether to do brotli response decompression or not. Defaults to false.
   *
   * @param enableBrotliDecompression whether or not to brotli decompress responses.
   * @return This builder.
   */
  fun enableBrotliDecompression(enableBrotliDecompression: Boolean): EngineBuilder {
    this.enableBrotliDecompression = enableBrotliDecompression
    return this
  }

  /**
   * Configure QUIC port migration. Defaults to disabled.
   *
   * @param numTimeoutsToTriggerPortMigration number of timeouts to trigger port migration. If 0,
   *   port migration is disabled.
   * @return This builder.
   */
  fun setNumTimeoutsToTriggerPortMigration(numTimeoutsToTriggerPortMigration: Int): EngineBuilder {
    this.numTimeoutsToTriggerPortMigration = numTimeoutsToTriggerPortMigration
    return this
  }

  /**
   * Specify whether to support socket tagging or not. Defaults to false.
   *
   * @param enableSocketTagging whether or not support socket tagging.
   * @return This builder.
   */
  fun enableSocketTagging(enableSocketTagging: Boolean): EngineBuilder {
    this.enableSocketTagging = enableSocketTagging
    return this
  }

  /**
   * Specify whether sockets may attempt to bind to a specific interface, based on network
   * conditions.
   *
   * @param enableInterfaceBinding whether to allow interface binding.
   * @return This builder.
   */
  fun enableInterfaceBinding(enableInterfaceBinding: Boolean): EngineBuilder {
    this.enableInterfaceBinding = enableInterfaceBinding
    return this
  }

  /**
   * Specify whether system proxy settings should be respected. If yes, Envoy Mobile will use
   * Android APIs to query Android Proxy settings configured on a device and will respect these
   * settings when establishing connections with remote services.
   *
   * The method is introduced for experimentation purposes and as a safety guard against critical
   * issues in the implementation of the proxying feature. It's intended to be removed after it's
   * confirmed that proxies on Android work as expected.
   *
   * @param enableProxying whether to enable Envoy's support for proxies.
   * @return This builder.
   */
  fun enableProxying(enableProxying: Boolean): EngineBuilder {
    this.enableProxying = enableProxying
    return this
  }

  /**
   * Add a rate at which to ping h2 connections on new stream creation if the connection has sat
   * idle. Defaults to 1 millisecond which effectively enables h2 ping functionality and results in
   * a connection ping on every new stream creation. Set it to 100000000 milliseconds to effectively
   * disable the ping.
   *
   * @param idleIntervalMs rate in milliseconds.
   * @return this builder.
   */
  fun addH2ConnectionKeepaliveIdleIntervalMilliseconds(idleIntervalMs: Int): EngineBuilder {
    this.h2ConnectionKeepaliveIdleIntervalMilliseconds = idleIntervalMs
    return this
  }

  /**
   * Add a rate at which to timeout h2 pings.
   *
   * @param timeoutSeconds rate in seconds to timeout h2 pings.
   * @return this builder.
   */
  fun addH2ConnectionKeepaliveTimeoutSeconds(timeoutSeconds: Int): EngineBuilder {
    this.h2ConnectionKeepaliveTimeoutSeconds = timeoutSeconds
    return this
  }

  /**
   * Set the maximum number of connections to open to a single host. Default is 7.
   *
   * @param maxConnectionsPerHost the maximum number of connections per host.
   * @return this builder.
   */
  fun setMaxConnectionsPerHost(maxConnectionsPerHost: Int): EngineBuilder {
    this.maxConnectionsPerHost = maxConnectionsPerHost
    return this
  }

  /**
   * Add a custom idle timeout for HTTP streams. Defaults to 15 seconds.
   *
   * @param streamIdleTimeoutSeconds idle timeout for HTTP streams.
   * @return this builder.
   */
  fun addStreamIdleTimeoutSeconds(streamIdleTimeoutSeconds: Int): EngineBuilder {
    this.streamIdleTimeoutSeconds = streamIdleTimeoutSeconds
    return this
  }

  /**
   * Add a custom per try idle timeout for HTTP streams. Defaults to 15 seconds.
   *
   * @param perTryIdleTimeoutSeconds per try idle timeout for HTTP streams.
   * @return this builder.
   */
  fun addPerTryIdleTimeoutSeconds(perTryIdleTimeoutSeconds: Int): EngineBuilder {
    this.perTryIdleTimeoutSeconds = perTryIdleTimeoutSeconds
    return this
  }

  /**
   * Add an HTTP filter factory used to create platform filters for streams sent by this client.
   *
   * @param name Custom name to use for this filter factory. Useful for having more meaningful trace
   *   logs, but not required. Should be unique per factory registered.
   * @param factory closure returning an instantiated filter.
   * @return this builder.
   */
  fun addPlatformFilter(name: String, factory: () -> Filter): EngineBuilder {
    this.platformFilterChain.add(FilterFactory(name, factory))
    return this
  }

  /**
   * Add an HTTP filter factory used to create platform filters for streams sent by this client.
   *
   * @param factory closure returning an instantiated filter.
   * @return this builder.
   */
  fun addPlatformFilter(factory: () -> Filter): EngineBuilder {
    this.platformFilterChain.add(FilterFactory(UUID.randomUUID().toString(), factory))
    return this
  }

  /**
   * Add an HTTP filter config used to create native filters for streams sent by this client.
   *
   * @param name Custom name to use for this filter factory. Useful for having more meaningful trace
   *   logs, but not required. Should be unique per filter.
   * @param typedConfig config string for the filter.
   * @return this builder.
   */
  fun addNativeFilter(
    name: String = UUID.randomUUID().toString(),
    typedConfig: String
  ): EngineBuilder {
    this.nativeFilterChain.add(EnvoyNativeFilterConfig(name, typedConfig))
    return this
  }

  /**
   * Set a closure to be called when the engine finishes its async startup and begins running.
   *
   * @param closure the closure to be called.
   * @return this builder.
   */
  fun setOnEngineRunning(closure: () -> Unit): EngineBuilder {
    this.onEngineRunning = closure
    return this
  }

  /**
   * Set a closure to be called when the engine's logger logs.
   *
   * @param closure: The closure to be called.
   * @return This builder.
   */
  fun setLogger(closure: (LogLevel, String) -> Unit): EngineBuilder {
    this.logger = closure
    return this
  }

  /** Set event tracker for the engine to call when it emits an event. */
  fun setEventTracker(eventTracker: (Map<String, String>) -> Unit): EngineBuilder {
    this.eventTracker = eventTracker
    return this
  }

  /**
   * Add a string accessor to this Envoy Client.
   *
   * @param name the name of the accessor.
   * @param accessor the string accessor.
   * @return this builder.
   */
  fun addStringAccessor(name: String, accessor: () -> String): EngineBuilder {
    this.stringAccessors.put(name, EnvoyStringAccessorAdapter(StringAccessor(accessor)))
    return this
  }

  /**
   * Register a key-value store implementation for internal use.
   *
   * @param name the name of the KV store.
   * @param keyValueStore the KV store implementation.
   * @return this builder.
   */
  fun addKeyValueStore(name: String, keyValueStore: KeyValueStore): EngineBuilder {
    this.keyValueStores.put(name, keyValueStore)
    return this
  }

  /**
   * Add the App Version of the App using this Envoy Client.
   *
   * @param appVersion the version.
   * @return this builder.
   */
  fun addAppVersion(appVersion: String): EngineBuilder {
    this.appVersion = appVersion
    return this
  }

  /**
   * Add the App ID of the App using this Envoy Client.
   *
   * @param appId the ID.
   * @return this builder.
   */
  fun addAppId(appId: String): EngineBuilder {
    this.appId = appId
    return this
  }

  /**
   * Set how the TrustChainVerification must be handled.
   *
   * @param trustChainVerification whether to mute TLS Cert verification - intended for testing
   * @return this builder.
   */
  fun setTrustChainVerification(trustChainVerification: TrustChainVerification): EngineBuilder {
    this.trustChainVerification = trustChainVerification
    return this
  }

  /**
   * Sets the upstream TLS socket's SNI override. If empty, no SNI override will be configured.
   *
   * @param sni the SNI.
   * @return this builder.
   */
  fun setUpstreamTlsSni(sni: String): EngineBuilder {
    this.upstreamTlsSni = sni
    return this
  }

  /**
   * Adds a runtime guard for the `envoy.reloadable_features.<guard>`. For example if the runtime
   * guard is `envoy.reloadable_features.use_foo`, the guard name is `use_foo`.
   *
   * @param name the name of the runtime guard, e.g. test_feature_false.
   * @param value the value for the runtime guard.
   * @return This builder.
   */
  fun addRuntimeGuard(name: String, value: Boolean): EngineBuilder {
    this.runtimeGuards.put(name, value)
    return this
  }

  /**
   * Add a host port pair that's known to speak QUIC.
   *
   * @param host the host's name.
   * @param port the port number.
   * @return This builder.
   */
  fun addQuicHint(host: String, port: Int): EngineBuilder {
    this.quicHints.put(host, port)
    return this
  }

  /**
   * Add a host suffix that's known to speak QUIC.
   *
   * @param suffix the suffix string.
   * @return This builder.
   */
  fun addQuicCanonicalSuffix(suffix: String): EngineBuilder {
    this.quicCanonicalSuffixes.add(suffix)
    return this
  }

  fun addH3ConnectionKeepaliveInitialIntervalMilliseconds(interval: Int): EngineBuilder {
    this.h3ConnectionKeepaliveInitialIntervalMilliseconds = interval
    return this
  }

  /**
   * Builds and runs a new Engine instance with the provided configuration.
   *
   * @return A new instance of Envoy.
   */
  @Suppress("LongMethod")
  fun build(): Engine {
    val engineConfiguration =
      EnvoyConfiguration(
        connectTimeoutSeconds,
        disableDnsRefreshOnFailure,
        disableDnsRefreshOnNetworkChange,
        dnsRefreshSeconds,
        dnsFailureRefreshSecondsBase,
        dnsFailureRefreshSecondsMax,
        dnsQueryTimeoutSeconds,
        dnsMinRefreshSeconds,
        dnsPreresolveHostnames,
        enableDNSCache,
        dnsCacheSaveIntervalSeconds,
        dnsNumRetries ?: -1,
        enableDrainPostDnsRefresh,
        enableHttp3,
        http3ConnectionOptions,
        http3ClientConnectionOptions,
        quicHints,
        quicCanonicalSuffixes,
        enableGzipDecompression,
        enableBrotliDecompression,
        numTimeoutsToTriggerPortMigration,
        enableSocketTagging,
        enableInterfaceBinding,
        h2ConnectionKeepaliveIdleIntervalMilliseconds,
        h2ConnectionKeepaliveTimeoutSeconds,
        maxConnectionsPerHost,
        streamIdleTimeoutSeconds,
        perTryIdleTimeoutSeconds,
        appVersion,
        appId,
        trustChainVerification,
        nativeFilterChain,
        platformFilterChain,
        stringAccessors,
        keyValueStores,
        runtimeGuards,
        enablePlatformCertificatesValidation,
        upstreamTlsSni,
        h3ConnectionKeepaliveInitialIntervalMilliseconds,
      )

    return EngineImpl(engineType(), engineConfiguration, logLevel)
  }

  /**
   * Add a specific implementation of `EnvoyEngine` to use for starting Envoy.
   *
   * A new instance of this engine will be created when `build()` is called.
   */
  fun addEngineType(engineType: () -> EnvoyEngine): EngineBuilder {
    this.engineType = engineType
    return this
  }

  /**
   * Specify whether to use platform provided certificate validation APIs or Envoy built-in
   * validation logic. Defaults to false.
   *
   * @param enablePlatformCertificatesValidation true if using platform APIs is desired.
   * @return This builder.
   */
  fun enablePlatformCertificatesValidation(
    enablePlatformCertificatesValidation: Boolean
  ): EngineBuilder {
    this.enablePlatformCertificatesValidation = enablePlatformCertificatesValidation
    return this
  }
}
