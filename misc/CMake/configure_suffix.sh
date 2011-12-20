     --prefix=*)   	          inputstr=$1;
     		   	          options="$options -DCMAKE_INSTALL_PREFIX=${inputstr#--prefix=}";
     		   	          shift;;
     *) 	   	          echo "Warning: unknown option $1";
     		   	          shift;;
   esac
done 
echo cmake $options
cmake $options
