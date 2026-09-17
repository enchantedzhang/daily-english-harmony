message("-- build BSS Java")

set(JAVA_BSS_PATH ${SRC_DIR}/src/plugin)
set(BSS_OUTPUT_DIR_NAME BoostKit-omnistatestore_1.1.0)
set(VERSION_1_16_3 flink-1.16.3)
set(VERSION_1_17_1 flink-1.17.1)
set(VERSION_1_16_1 flink-1.16.1)
set(VERSION_1_20_0 flink-1.20.0)
add_custom_target(build_bss_java_version_1.16.3
        COMMAND mvn clean package -Drevision=1.1.0-SNAPSHOT -Dmaven.test.skip=true -P${VERSION_1_16_3} -Dmaven.compiler.fork=true
        -Dmaven.wagon.http.ssl.insecure=true -Dmaven.wagon.http.ssl.allowall=true
        WORKING_DIRECTORY ${JAVA_BSS_PATH}
        )

add_custom_target(build_bss_install_java_version_1.16.3
        COMMAND mkdir -p ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars
        COMMAND cp ${JAVA_BSS_PATH}/state_store_all/target/maven-assembly-plugin/flink-boost-statebackend-1.1.0-SNAPSHOT-for-${VERSION_1_16_3}.jar ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars
        )

add_custom_target(build_bss_java_version_1.17.1
        COMMAND mvn clean package -Drevision=1.1.0-SNAPSHOT -Dmaven.test.skip=true -P${VERSION_1_17_1} -Dmaven.compiler.fork=true
        -Dmaven.wagon.http.ssl.insecure=true -Dmaven.wagon.http.ssl.allowall=true
        WORKING_DIRECTORY ${JAVA_BSS_PATH}
)

add_custom_target(build_bss_install_java_version_1.17.1
        COMMAND mkdir -p ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars
        COMMAND cp ${JAVA_BSS_PATH}/state_store_all/target/maven-assembly-plugin/flink-boost-statebackend-1.1.0-SNAPSHOT-for-${VERSION_1_17_1}.jar ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars
)

add_custom_target(build_bss_java_version_1.16.1
        COMMAND mvn clean package -Drevision=1.1.0-SNAPSHOT -Dmaven.test.skip=true -P${VERSION_1_16_1} -Dmaven.compiler.fork=true
        -Dmaven.wagon.http.ssl.insecure=true -Dmaven.wagon.http.ssl.allowall=true
        WORKING_DIRECTORY ${JAVA_BSS_PATH}
)

add_custom_target(build_bss_install_java_version_1.16.1
        COMMAND mkdir -p ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars
        COMMAND cp ${JAVA_BSS_PATH}/state_store_all/target/maven-assembly-plugin/flink-boost-statebackend-1.1.0-SNAPSHOT-for-${VERSION_1_16_1}.jar ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars
)

add_custom_target(build_bss_java_version_1.20.0
        COMMAND mvn clean package -Drevision=1.1.0-SNAPSHOT -Dmaven.test.skip=true -P${VERSION_1_20_0} -Dmaven.compiler.fork=true
        -Dmaven.wagon.http.ssl.insecure=true -Dmaven.wagon.http.ssl.allowall=true
        WORKING_DIRECTORY ${JAVA_BSS_PATH}
)

add_custom_target(build_bss_install_java_version_1.20.0
        COMMAND mkdir -p ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars
        COMMAND cp ${JAVA_BSS_PATH}/state_store_all/target/maven-assembly-plugin/flink-boost-statebackend-1.1.0-SNAPSHOT-for-${VERSION_1_20_0}.jar ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars
)