pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "LviLibUvcProject"
include(":app")
include(":opencv")
include(":DependenciesLib")
include(":LibUvcCamera")
include(":LibHidApi")
